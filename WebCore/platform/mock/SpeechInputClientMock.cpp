/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SpeechInputClientMock.h"

#if ENABLE(INPUT_SPEECH)

#include "SpeechInputListener.h"

namespace WebCore {

SpeechInputClientMock::SpeechInputClientMock()
    : m_recording(false)
    , m_timer(this, &SpeechInputClientMock::timerFired)
    , m_listener(0)
    , m_requestId(0)
{
}

void SpeechInputClientMock::setListener(SpeechInputListener* listener)
{
    m_listener = listener;
}

bool SpeechInputClientMock::startRecognition(int requestId, const IntRect& elementRect, const AtomicString& language, const String& grammar)
{
    if (m_timer.isActive())
        return false;
    m_requestId = requestId;
    m_recording = true;
    m_language = language;
    m_timer.startOneShot(0);
    return true;
}

void SpeechInputClientMock::stopRecording(int requestId)
{
    ASSERT(requestId == m_requestId);
    if (m_timer.isActive() && m_recording) {
        m_timer.stop();
        timerFired(&m_timer);
    }
}

void SpeechInputClientMock::cancelRecognition(int requestId)
{
    ASSERT(requestId == m_requestId);
    if (m_timer.isActive()) {
        m_timer.stop();
        m_recording = false;
        m_listener->didCompleteRecognition(m_requestId);
        m_requestId = 0;
    }
}

void SpeechInputClientMock::setRecognitionResult(const String& result, const AtomicString& language)
{
    if (language.isEmpty())
        m_resultForEmptyLanguage = result;
    else
        m_recognitionResult.set(language, result);
}

void SpeechInputClientMock::clearResults()
{
    m_resultForEmptyLanguage = String();
    m_recognitionResult.clear();
}

void SpeechInputClientMock::timerFired(WebCore::Timer<SpeechInputClientMock>*)
{
    if (m_recording) {
        m_recording = false;
        m_listener->didCompleteRecording(m_requestId);
        m_timer.startOneShot(0);
    } else {
        SpeechInputResultArray results;
        bool noResultsFound = false;

        // Empty language case must be handled separately to avoid problems with HashMap and empty keys.
        if (m_language.isEmpty()) {
            if (!m_resultForEmptyLanguage.isNull())
                results.append(SpeechInputResult::create(m_resultForEmptyLanguage, 1.0));
            else
                noResultsFound = true;
        } else {
            if (m_recognitionResult.contains(m_language))
                results.append(SpeechInputResult::create(m_recognitionResult.get(m_language), 1.0));
            else
                noResultsFound = true;
        }

        if (noResultsFound) {
            // Can't avoid setting a result even if no result was set for the given language.
            // This would avoid generating the events used to check the results and the test would timeout.
            String error("error: no result found for language '");
            error.append(m_language);
            error.append("'");
            results.append(SpeechInputResult::create(error, 1.0));
        }

        m_listener->setRecognitionResult(m_requestId, results);
        m_listener->didCompleteRecognition(m_requestId);
        m_requestId = 0;
    }
}

} // namespace WebCore

#endif // ENABLE(INPUT_SPEECH)
