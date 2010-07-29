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
#include "WebSpeechInputController.h"
#include "WebString.h"
#include "WebViewClient.h"
#include "page/SpeechInputListener.h"

#if ENABLE(INPUT_SPEECH)

namespace WebKit {

SpeechInputClientImpl::SpeechInputClientImpl(WebViewClient* web_view_client)
    : m_controller(web_view_client->speechInputController(this))
    , m_listener(0)
{
    // FIXME: Right now WebViewClient gives a null pointer, and with the
    // runtime flag for speech input feature set to true by default this will
    // always assert. Enable this assert once the WebViewClient starts to
    // give a valid pointer.
    // ASSERT(m_controller);
}

SpeechInputClientImpl::~SpeechInputClientImpl()
{
}

bool SpeechInputClientImpl::startRecognition(WebCore::SpeechInputListener* listener)
{
    m_listener = listener;
    return m_controller->startRecognition();
}

void SpeechInputClientImpl::stopRecording()
{
    ASSERT(m_listener);
    m_controller->stopRecording();
}

void SpeechInputClientImpl::cancelRecognition()
{
    ASSERT(m_listener);
    m_controller->cancelRecognition();
}

void SpeechInputClientImpl::didCompleteRecording()
{
    ASSERT(m_listener);
    m_listener->didCompleteRecording();
}

void SpeechInputClientImpl::didCompleteRecognition()
{
    ASSERT(m_listener);
    m_listener->didCompleteRecognition();
    m_listener = 0;
}

void SpeechInputClientImpl::setRecognitionResult(const WebString& result)
{
    ASSERT(m_listener);
    m_listener->setRecognitionResult(result);
}

} // namespace WebKit

#endif // ENABLE(INPUT_SPEECH)
