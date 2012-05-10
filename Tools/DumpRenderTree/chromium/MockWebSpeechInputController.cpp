/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockWebSpeechInputController.h"

#include "WebSpeechInputListener.h"
#include "platform/WebCString.h"
#include "platform/WebVector.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(INPUT_SPEECH)

using namespace WebKit;

PassOwnPtr<MockWebSpeechInputController> MockWebSpeechInputController::create(WebSpeechInputListener* listener)
{
    return adoptPtr(new MockWebSpeechInputController(listener));
}

void MockWebSpeechInputController::addMockRecognitionResult(const WebString& result, double confidence, const WebString& language)
{
    WebSpeechInputResult res;
    res.assign(result, confidence);

    if (language.isEmpty())
        m_resultsForEmptyLanguage.append(res);
    else {
        String langString = String::fromUTF8(language.utf8().data());
        if (!m_recognitionResults.contains(langString))
            m_recognitionResults.set(langString, Vector<WebSpeechInputResult>());
        m_recognitionResults.find(langString)->second.append(res);
    }
}

void MockWebSpeechInputController::setDumpRect(bool dumpRect)
{
    m_dumpRect = dumpRect;
}

void MockWebSpeechInputController::clearResults()
{
    m_resultsForEmptyLanguage.clear();
    m_recognitionResults.clear();
    m_dumpRect = false;
}

bool MockWebSpeechInputController::startRecognition(int requestId, const WebRect& elementRect, const WebString& language, const WebString& grammar, const WebSecurityOrigin& origin)
{
    if (m_speechTask)
        return false;

    m_requestId = requestId;
    m_requestRect = elementRect;
    m_recording = true;
    m_language = String::fromUTF8(language.utf8().data());

    m_speechTask = new SpeechTask(this);
    postTask(m_speechTask);

    return true;
}

void MockWebSpeechInputController::cancelRecognition(int requestId)
{
    if (m_speechTask) {
        ASSERT(requestId == m_requestId);

        m_speechTask->stop();
        m_recording = false;
        m_listener->didCompleteRecognition(m_requestId);
        m_requestId = 0;
    }
}

void MockWebSpeechInputController::stopRecording(int requestId)
{
    ASSERT(requestId == m_requestId);
    if (m_speechTask && m_recording) {
        m_speechTask->stop();
        speechTaskFired();
    }
}

MockWebSpeechInputController::MockWebSpeechInputController(WebSpeechInputListener* listener)
    : m_listener(listener)
    , m_speechTask(0)
    , m_recording(false)
    , m_requestId(-1)
    , m_dumpRect(false)
{
}

static WebSpeechInputResultArray makeRectResult(const WebRect& rect)
{
    StringBuilder sb;
    sb.append(String::number(rect.x));
    sb.append(",");
    sb.append(String::number(rect.y));
    sb.append(",");
    sb.append(String::number(rect.width));
    sb.append(",");
    sb.append(String::number(rect.height));

    WebSpeechInputResult res;
    res.assign(WebString(sb.characters(), sb.length()), 1.0);

    WebSpeechInputResultArray results;
    results.assign(&res, 1);
    return results;
}

void MockWebSpeechInputController::speechTaskFired()
{
    if (m_recording) {
        m_recording = false;
        m_listener->didCompleteRecording(m_requestId);

        m_speechTask = new SpeechTask(this);
        postTask(m_speechTask);
    } else {
        bool noResultsFound = false;
        // We take a copy of the requestId here so that if scripts destroyed the input element
        // inside one of the callbacks below, we'll still know what this session's requestId was.
        int requestId = m_requestId;
        m_requestId = 0;

        if (m_dumpRect) {
            m_listener->setRecognitionResult(requestId, makeRectResult(m_requestRect));
        } else if (m_language.isEmpty()) {
            // Empty language case must be handled separately to avoid problems with HashMap and empty keys.
            if (!m_resultsForEmptyLanguage.isEmpty())
                m_listener->setRecognitionResult(requestId, m_resultsForEmptyLanguage);
            else
                noResultsFound = true;
        } else {
            if (m_recognitionResults.contains(m_language))
                m_listener->setRecognitionResult(requestId, m_recognitionResults.get(m_language));
            else
                noResultsFound = true;
        }

        if (noResultsFound) {
            // Can't avoid setting a result even if no result was set for the given language.
            // This would avoid generating the events used to check the results and the test would timeout.
            String error("error: no result found for language '");
            error.append(m_language);
            error.append("'");

            WebSpeechInputResult res;
            res.assign(WebString::fromUTF8(error.utf8().data()), 1.0);

            Vector<WebSpeechInputResult> results;
            results.append(res);

            m_listener->setRecognitionResult(requestId, results);
        }
    }
}

MockWebSpeechInputController::SpeechTask::SpeechTask(MockWebSpeechInputController* mock)
    : MethodTask<MockWebSpeechInputController>::MethodTask(mock)
{
}

void MockWebSpeechInputController::SpeechTask::stop()
{
    m_object->m_speechTask = 0;
    cancel();
    delete(this);
}

void MockWebSpeechInputController::SpeechTask::runIfValid()
{
    m_object->m_speechTask = 0;
    m_object->speechTaskFired();
}

#endif
