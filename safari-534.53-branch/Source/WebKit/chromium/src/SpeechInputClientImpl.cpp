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
#include "SpeechInputClientImpl.h"

#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "SpeechInputListener.h"
#include "WebSecurityOrigin.h"
#include "WebSpeechInputController.h"
#include "WebString.h"
#include "WebViewClient.h"
#include <wtf/PassOwnPtr.h>

#if ENABLE(INPUT_SPEECH)

namespace WebKit {

PassOwnPtr<SpeechInputClientImpl> SpeechInputClientImpl::create(WebViewClient* client)
{
    return adoptPtr(new SpeechInputClientImpl(client));
}

SpeechInputClientImpl::SpeechInputClientImpl(WebViewClient* web_view_client)
    : m_controller(web_view_client ? web_view_client->speechInputController(this) : 0)
    , m_listener(0)
{
}

SpeechInputClientImpl::~SpeechInputClientImpl()
{
}

void SpeechInputClientImpl::setListener(WebCore::SpeechInputListener* listener)
{
    m_listener = listener;
}

bool SpeechInputClientImpl::startRecognition(int requestId, const WebCore::IntRect& elementRect, const AtomicString& language, const String& grammar, WebCore::SecurityOrigin* origin)
{
    ASSERT(m_listener);
    return m_controller->startRecognition(requestId, elementRect, language, grammar, WebSecurityOrigin(origin));
}

void SpeechInputClientImpl::stopRecording(int requestId)
{
    ASSERT(m_listener);
    m_controller->stopRecording(requestId);
}

void SpeechInputClientImpl::cancelRecognition(int requestId)
{
    ASSERT(m_listener);
    m_controller->cancelRecognition(requestId);
}

void SpeechInputClientImpl::didCompleteRecording(int requestId)
{
    ASSERT(m_listener);
    m_listener->didCompleteRecording(requestId);
}

void SpeechInputClientImpl::didCompleteRecognition(int requestId)
{
    ASSERT(m_listener);
    m_listener->didCompleteRecognition(requestId);
}

void SpeechInputClientImpl::setRecognitionResult(int requestId, const WebSpeechInputResultArray& results)
{
    ASSERT(m_listener);
    WebCore::SpeechInputResultArray webcoreResults(results.size());
    for (size_t i = 0; i < results.size(); ++i)
        webcoreResults[i] = results[i];
    m_listener->setRecognitionResult(requestId, webcoreResults);
}

} // namespace WebKit

#endif // ENABLE(INPUT_SPEECH)
