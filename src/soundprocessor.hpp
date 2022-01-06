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

#ifndef SoundProcessor_HPP_
#define SoundProcessor_HPP_

#include <bb/cascades/Application>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/asoundlib.h>
#include "kiss_fft.h"
#include "kiss_fftr.h"

#define SUCCESS 0
#define FAILURE -1
#define F_PI 3.14159265f

#define DETECT_OVERTONES

class SoundProcessor : public QObject {
    Q_OBJECT

public:
    struct NoteInfo {
    	char note[16];
    	float centsDiff;
    	float amplitude;
    	float frequency;
    };

    SoundProcessor(QObject *parent = 0);
    virtual ~SoundProcessor();

    int init(const char*);
    int terminate();

Q_SIGNALS:
    void readingUpdated(SoundProcessor::NoteInfo);

public Q_SLOTS:
    void readPCM();

private:
    NoteInfo getNote();
    void applyWindow(float*, int);
    void convertFreqToNote(float, float, int, struct NoteInfo*);
    inline float getAmplitude(kiss_fft_cpx);
    inline float convertBinToFreq(int);
    inline int convertFreqToBin(float);
    void getParabolicInterpolationVertex(float, float, float, float, float, float, float*, float*);
    float getOvertone(kiss_fft_cpx*, float, int);

private:
    int card;
    QSocketNotifier* fdNotifier;
    char* fragBuff;
    char* buff;
    float* history;
    size_t historySize;
    int readCount;
    int silentReadCount;
    snd_pcm_t* pcmHandle;
    snd_pcm_info_t pcmInfo;
    snd_pcm_format_t pcmFormat;
    snd_pcm_channel_params_t pcmParams;
    snd_pcm_channel_setup_t pcmSetup;
    snd_pcm_channel_info_t pcmChannelInfo;

    snd_mixer_t* mixerHandle;
    snd_mixer_group_t group;

    int tuningFreq;
    int sampleRate;
    int sampleChannels;
    int sampleBits;
    int fragSize;

    size_t captureWindow;
    size_t fftSize;
    size_t sampleReplications;
};
#endif /* SoundProcessor_HPP_ */
