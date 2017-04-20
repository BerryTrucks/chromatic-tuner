/*
 * Copyright (c) 2016-2017 Theodore Opeiko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "soundprocessor.hpp"

SoundProcessor::SoundProcessor(QObject *parent) {
    setParent(parent);
    tuningFreq = 440;
    sampleRate = 44100;
    card = -1;
	init("pcmPreferred");
}

SoundProcessor::~SoundProcessor() {
	terminate();
}

int SoundProcessor::init(const char * name) {
	int rtn;
	char *devName;

	if (name == NULL) {
		devName = (char*)"pcmPreferred";
	} else {
		devName = (char*)name;
	}

	if ((rtn = snd_pcm_open_name(&pcmHandle, devName, SND_PCM_OPEN_CAPTURE)) < 0) {
		qDebug("snd_pcm_open_name failed: %s\n", snd_strerror(rtn));
		return FAILURE;
	}

	if ((rtn = snd_pcm_info(pcmHandle, &pcmInfo)) < 0) {
		qDebug("snd_pcm_info failed: %s\n", snd_strerror(rtn));
		return FAILURE;
	}
	card = pcmInfo.card;

	memset(&pcmChannelInfo, 0, sizeof(pcmChannelInfo));
	pcmChannelInfo.channel = SND_PCM_CHANNEL_CAPTURE;
	if ((rtn = snd_pcm_plugin_info(pcmHandle, &pcmChannelInfo)) < 0) {
		qDebug("snd_pcm_plugin_info failed: %s\n", snd_strerror(rtn));
		return FAILURE;
	}

	memset(&pcmParams, 0, sizeof(pcmParams));

	pcmParams.mode = SND_PCM_MODE_BLOCK;
	pcmParams.channel = SND_PCM_CHANNEL_CAPTURE;
	pcmParams.start_mode = SND_PCM_START_DATA;
	pcmParams.stop_mode = SND_PCM_STOP_ROLLOVER;
	pcmParams.buf.block.frag_size = pcmChannelInfo.max_fragment_size;

	pcmParams.buf.block.frags_max = 0;
	pcmParams.buf.block.frags_min = 1;

	pcmParams.format.rate = sampleRate;
	// other format options: SND_PCM_SFMT_S32_LE SND_PCM_FMT_U16_LE SND_PCM_SFMT_S24
	pcmParams.format.format = SND_PCM_SFMT_S16_LE;
	pcmParams.format.voices = 1;

	if ((rtn = snd_pcm_plugin_params(pcmHandle, &pcmParams)) < 0) {
		qDebug("snd_pcm_plugin_params failed: %s\n", snd_strerror(rtn));
		return FAILURE;
	}

	fragSize = pcmParams.buf.block.frag_size;

	if ((rtn = snd_pcm_channel_prepare(pcmHandle, SND_PCM_CHANNEL_CAPTURE)) < 0) {
		qDebug("snd_pcm_channel_prepare failed: %s\n", snd_strerror(rtn));
		return FAILURE;
	}

	// FFT related configuration
	fftSize = 131072;
	captureWindow = (size_t)ceil((float)fftSize/(float)fragSize)*fragSize;
	fragBuff = new char[fragSize];
	memset(fragBuff, 0, fragSize);
	buff = new char[captureWindow];
	memset(buff, 0, captureWindow);

	historySize = 3;
    history = new float[historySize];
    memset(history, 0, sizeof(float)*historySize);

    readCount = 0;

    qDebug("Fragment size: %d", fragSize);
    qDebug("Capture window size: %d", captureWindow);
    qDebug("FFT size: %d", fftSize);

	// connect fd listener
	int pcmfd = snd_pcm_file_descriptor(pcmHandle, SND_PCM_CHANNEL_CAPTURE);
	fdNotifier = new QSocketNotifier(pcmfd, QSocketNotifier::Read);
	connect(fdNotifier, SIGNAL(activated(int)), this, SLOT(readPCM()));

	return SUCCESS;
}

void SoundProcessor::readPCM() {
    ssize_t bytesRead = snd_pcm_read(pcmHandle, fragBuff, fragSize);
    //qDebug("Bytes read: %d", bytesRead);

    // Add read bytes to the end of the buffer, remove old data from the beginning of
    // the buffer.
    // TODO: replace with ring buffer
    if (bytesRead > 0) {
        char* tmp = new char[captureWindow-bytesRead];
        memcpy(&tmp[0], &buff[bytesRead], captureWindow-bytesRead);
        memcpy(&buff[0], &tmp[0], captureWindow - bytesRead);
        memcpy(&buff[captureWindow-bytesRead], fragBuff, bytesRead);
        readCount++;
        delete tmp;
    }

    // Don't analyse every single reading
    if (readCount == 2) {
        NoteInfo note = getNote();
        emit readingUpdated(note);

        readCount = 0;
    }
}

SoundProcessor::NoteInfo SoundProcessor::getNote() {
    struct NoteInfo note;
    note.note[0] = 0;
    note.centsDiff = 0.0f;

    kiss_fft_scalar *fftIn = new kiss_fft_scalar[fftSize];
    kiss_fft_cpx *fftOut = new kiss_fft_cpx[fftSize*2];
    kiss_fftr_cfg fft = kiss_fftr_alloc(fftSize, 0, 0, 0);
    memcpy(fftIn, buff, fftSize);
    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);

    size_t windowSize = fftSize;
    applyWindow(fftIn, windowSize);
    kiss_fftr(fft, fftIn, fftOut);

    clock_gettime(CLOCK_REALTIME, &t1);
    qDebug("FFT took %li sec + %f msec\n", long(t1.tv_sec) - long(t0.tv_sec), float(long(t1.tv_nsec) - long(t0.tv_nsec))/1000000);
    float maxAmplitude = 0;
    int bin = 0;
    for (size_t j=0; j < fftSize; j++) {
        float amplitude = getAmplitude(fftOut[j]);
        if (amplitude > maxAmplitude) {
            maxAmplitude = amplitude;
            bin = j;
        }
    }

    if (maxAmplitude > 40) {
        float freq = convertBinToFreq(bin);
        float adjustedFreq = freq;
        float adjustedAmplitude = maxAmplitude;
        int overtone = 1;
        silentReadCount = 0;

        // For small FFT sizes interpolation is required, the dominant bin's and adjacent bins'
        // amplitudes fluctuates as the actual frequency changes
        if (bin > 0 && fftSize <= 32768) {
            float freqL = convertBinToFreq(bin-1);
            float freqR = convertBinToFreq(bin+1);
            float amplitudeL = getAmplitude(fftOut[bin-1]);
            float amplitudeR = getAmplitude(fftOut[bin+1]);

            // Linear interpolation crudely approximates the frequency from two adjacent bins
            //adjustedFreq = freq + (freqR - freq)*(amplitudeR/maxAmplitude) + (freqL - freq)*(amplitudeL/maxAmplitude);

            // Approximate the frequency based on two adjacent bins using a quadratic curve
            getParabolicInterpolationVertex(freqL, amplitudeL, freq, maxAmplitude, freqR, amplitudeR, &adjustedFreq, &adjustedAmplitude);
        }

        // Detect if we caught an overtone instead of the fundamental
        //overtone = getOvertone(fftOut, adjustedFreq, bin);

        // Push older values in history of captured frequencies back in the history array
        for (size_t h = 1; h < historySize; h++)
            history[h-1] = history[h];
        history[historySize - 1] = adjustedFreq;

        bool consistentTone = true;
        bool saneFreqRange = freq >= 20 && freq <= 22000;
        for (size_t h = 1; h < historySize; h++) {
            float freq = history[h];
            float prevFreq = history[h-1];
            if (freq > prevFreq*1.1 || freq < prevFreq*0.9) {
                consistentTone = false;
                break;
            }
        }

        // Sanity check to filter out unstable readings. If reading is not stable, return an empty note
        if (consistentTone && saneFreqRange) {
            //convertFreqToNote(freq, maxAmplitude, overtone, &lastStableNote);
            convertFreqToNote(adjustedFreq, maxAmplitude, overtone, &note);
            qDebug("f=%f a=%f %s", freq, maxAmplitude, note.note);
        } else {
            note.note[0] = 0;
            note.centsDiff = 0.0f;
        }
    } else {
        // Maximum signal amplitude is too low
        silentReadCount++;
    }

    // cleanup
    kiss_fft_free(fft);
    delete fftIn;
    delete fftOut;

    return note;
}

int SoundProcessor::terminate() {
    disconnect(fdNotifier, SIGNAL(activated(int)), this, SLOT(readPCM()));
    delete fdNotifier;

	int rtn;
	if ((rtn = snd_pcm_close(pcmHandle)) < 0) {
		qDebug("snd_pcm_close failed: %s\n", snd_strerror(rtn));
		return FAILURE;
	}
	delete buff;
	delete fragBuff;
	delete history;

	return SUCCESS;
}

void SoundProcessor::applyWindow(short* data, int n) {
	for (int i = 0; i < n; i++) {
		float coef = 0.5*(1 - cos(2*F_PI*i/(n-1)));
		data[i] = short(coef*float(data[i]));
	}
}

void SoundProcessor::convertFreqToNote(float f, float a, int overtone, struct NoteInfo* description) {
	const char* noteNames[] = { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };
	float linearF = log2((f/overtone)/tuningFreq) + 4;
	float octave = floor(linearF);
	float centsTotal = 1200*(linearF - octave);
	int noteNumber = int(floor(centsTotal/100)) % 12;
	float centsOff = centsTotal - noteNumber*100;
	if (centsOff > 50) {
		noteNumber++;
		if (noteNumber >= 12) {
			noteNumber = noteNumber % 12;
			octave++;
		}
		centsTotal = 1200*(linearF - octave);
		centsOff = centsTotal - noteNumber*100;
	}
	sprintf(description->note, "%s%d", noteNames[noteNumber], int(octave));
	description->centsDiff = centsOff;
	description->frequency = f;
	description->amplitude = a;
}

inline float SoundProcessor::getAmplitude(kiss_fft_cpx cpx) {
	return sqrt(pow(cpx.r, 2) + pow(cpx.i, 2));
}

inline float SoundProcessor::convertBinToFreq(int bin) {
	return ((bin-1)*(sampleRate/2))/(fftSize/2);
}

inline int SoundProcessor::convertFreqToBin(float f) {
	return int(floor(f*(fftSize/2)/(sampleRate/2))) + 2;
}

void SoundProcessor::getParabolicInterpolationVertex(float x1, float y1, float x2, float y2, float x3, float y3, float* xv, float* yv) {
	double d = (x1 - x2) * (x1 - x3) * (x2 - x3);
	double a = (x3 * (y2 - y1) + x2 * (y1 - y3) + x1 * (y3 - y2)) / d;
	double b = (x3*x3 * (y1 - y2) + x2*x2 * (y3 - y1) + x1*x1 * (y2 - y3)) / d;
	double c = (x2 * x3 * (x2 - x3) * y1 + x3 * x1 * (x3 - x1) * y2 + x1 * x2 * (x1 - x2) * y3) / d;

	*xv = b*(-1) / (2*a);
	*yv = c - b*b / (4*a);
}

float SoundProcessor::getOvertone(kiss_fft_cpx *fft, float freq, int bin) {
    float overtone = 1;
    if (bin > 1) {
        // check if the bin caught an overtone
        float fundamentalAmplitude = 0.0f;
        for (float m = 1.5; m <= 2.0; m += 0.5) {
            float a = 0;
            int binGuess = convertFreqToBin(freq/m);
            //int b = binGuess;
            for (int b = binGuess -1; (b <= binGuess + 1) && (b > 0); b++) {
                a = getAmplitude(fft[b]);
                if (a > fundamentalAmplitude) {
                    fundamentalAmplitude = a;
                    overtone = m;
                }
            }
            //qDebug("f/%f amplitude=%f\n", m, a);
        }
        if (fundamentalAmplitude >= freq*0.5) {
            qDebug("Overtone detected: f*%f, a=%f\n", overtone, fundamentalAmplitude);
        } else {
            overtone = 1;
        }
    }

    return overtone;
}
