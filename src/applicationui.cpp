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

#include "applicationui.hpp"

#include <bb/cascades/Application>
#include <bb/cascades/Label>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/LocaleHandler>

using namespace bb::cascades;

ApplicationUI::ApplicationUI() : QObject()
{
    // prepare the localisation
    m_pTranslator = new QTranslator(this);
    m_pLocaleHandler = new LocaleHandler(this);
    QObject::connect(m_pLocaleHandler, SIGNAL(systemLanguageChanged()), this,
            SLOT(onSystemLanguageChanged()));

    // initial load
    onSystemLanguageChanged();

    // Create the main scene
    QmlDocument* qml = QmlDocument::create("asset:///qml/main.qml").parent(this);

    // Create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();

    // Set created root object as the application scene
    Application::instance()->setScene(root);

    // Sound capture will start
    soundProcessor = new SoundProcessor();
    QObject::connect(soundProcessor, SIGNAL(readingUpdated(SoundProcessor::NoteInfo)), this,
            SLOT(onReadingUpdated(SoundProcessor::NoteInfo)));
}

void ApplicationUI::onReadingUpdated(SoundProcessor::NoteInfo note) {
    QString readableNote(note.note);
    QString readableCents;
    readableCents.sprintf("%c%d\n", (note.centsDiff < 0? '-' : '+'), (int)fabs(note.centsDiff));

    Label* noteLabel = Application::instance()->findChild<Label*>("noteLabel");
    Label* offsetVisualLabel = Application::instance()->findChild<Label*>("tuneOffsetLabel");
    Label* offsetCentsLabel = Application::instance()->findChild<Label*>("tuneCentsOffsetLabel");

    if (noteLabel == NULL || offsetCentsLabel == NULL || offsetVisualLabel == NULL)
       return;

    // Human-readable note name
    if (readableNote.isEmpty()) {
        noteLabel->setText(QObject::tr("I'm listening..."));
    } else {
        noteLabel->setText(readableNote);
    }

    // Human-readable tuning offset in cents
    if (!readableCents.isEmpty() && !readableNote.isEmpty())
        offsetCentsLabel->setText(readableCents);
    else
        offsetCentsLabel->setText("");

    // Visual tuning offset using coloured circles
    QString flatIndicatorBigDiff = "";
    QString flatIndicatorSmallDiff = "";
    QString inTuneIndicator = "";
    QString sharpIndicatorSmallDiff = "";
    QString sharpIndicatorBigDiff = "";

    QString neutralIndicatorSpan = "<span style=\"color:#ffeee8d5\">n</span>";
    QString bigDiffIndicatorSpan = "<span style=\"color:#ffdc322f\">n</span>";
    QString smallDiffIndicatorSpan = "<span style=\"color:#ffb58900\">n</span>";
    QString inTuneIndicatorSpan = "<span style=\"color:#ff2aa198\">n</span>";

    int bigDiff = 25;
    int smallDiff = 5;

    if (note.centsDiff <= -1*bigDiff)
        flatIndicatorBigDiff += bigDiffIndicatorSpan;
    else
        flatIndicatorBigDiff += neutralIndicatorSpan;

    if (note.centsDiff > -1*bigDiff && note.centsDiff <= -1*smallDiff)
        flatIndicatorSmallDiff += smallDiffIndicatorSpan;
    else
        flatIndicatorSmallDiff += neutralIndicatorSpan;

    if (note.centsDiff > -1*smallDiff && note.centsDiff < smallDiff)
        inTuneIndicator += inTuneIndicatorSpan;
    else
        inTuneIndicator += neutralIndicatorSpan;

    if (note.centsDiff < bigDiff && note.centsDiff > smallDiff)
        sharpIndicatorSmallDiff += smallDiffIndicatorSpan;
    else
        sharpIndicatorSmallDiff += neutralIndicatorSpan;

    if (note.centsDiff >= bigDiff)
        sharpIndicatorBigDiff += bigDiffIndicatorSpan;
    else
        sharpIndicatorBigDiff += neutralIndicatorSpan;

    QString status = flatIndicatorBigDiff + " " + flatIndicatorSmallDiff + " "
            + inTuneIndicator + " " + sharpIndicatorSmallDiff + " " + sharpIndicatorBigDiff;

    offsetVisualLabel->setText(status);
}

void ApplicationUI::onSystemLanguageChanged()
{
    QCoreApplication::instance()->removeTranslator(m_pTranslator);
    // Initiate, load and install the application translation files.
    QString locale_string = QLocale().name();
    QString file_name = QString("Tuner_%1").arg(locale_string);
    if (m_pTranslator->load(file_name, "app/native/qm")) {
        QCoreApplication::instance()->installTranslator(m_pTranslator);
    }
}
