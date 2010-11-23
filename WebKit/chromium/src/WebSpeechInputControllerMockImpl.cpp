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
#include "WebSpeechInputControllerMockImpl.h"

#include "PlatformString.h"
#include "SpeechInputClientMock.h"
#include "WebRect.h"

namespace WebKit {

WebSpeechInputControllerMock* WebSpeechInputControllerMock::create(WebSpeechInputListener* listener)
{
    return new WebSpeechInputControllerMockImpl(listener);
}

WebSpeechInputControllerMockImpl::WebSpeechInputControllerMockImpl(
    WebSpeechInputListener* listener)
    : m_webcoreMock(new WebCore::SpeechInputClientMock())
    , m_listener(listener)
{
    m_webcoreMock->setListener(this);
}

WebSpeechInputControllerMockImpl::~WebSpeechInputControllerMockImpl()
{
    m_webcoreMock->setListener(0);
}

void WebSpeechInputControllerMockImpl::addMockRecognitionResult(const WebString& result, double confidence, const WebString &language)
{
    m_webcoreMock->addRecognitionResult(result, confidence, language);
}

void WebSpeechInputControllerMockImpl::clearResults()
{
    m_webcoreMock->clearResults();
}

void WebSpeechInputControllerMockImpl::didCompleteRecording(int requestId)
{
    m_listener->didCompleteRecording(requestId);
}

void WebSpeechInputControllerMockImpl::didCompleteRecognition(int requestId)
{
    m_listener->didCompleteRecognition(requestId);
}

void WebSpeechInputControllerMockImpl::setRecognitionResult(int requestId, const WebCore::SpeechInputResultArray& result)
{
    m_listener->setRecognitionResult(requestId, result);
}

bool WebSpeechInputControllerMockImpl::startRecognition(int requestId, const WebRect& elementRect, const WebString& language, const WebString& grammar)
{
    return m_webcoreMock->startRecognition(requestId, elementRect, language, grammar);
}

void WebSpeechInputControllerMockImpl::cancelRecognition(int requestId)
{
    m_webcoreMock->cancelRecognition(requestId);
}

void WebSpeechInputControllerMockImpl::stopRecording(int requestId)
{
    m_webcoreMock->stopRecording(requestId);
}

} // namespace WebKit
