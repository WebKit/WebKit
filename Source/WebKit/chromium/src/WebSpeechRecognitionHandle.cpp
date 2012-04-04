/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSpeechRecognitionHandle.h"

#include "SpeechRecognition.h"

using namespace WebCore;

namespace WebKit {

void WebSpeechRecognitionHandle::reset()
{
    m_private.reset();
}

void WebSpeechRecognitionHandle::assign(const WebSpeechRecognitionHandle& other)
{
    m_private = other.m_private;
}

bool WebSpeechRecognitionHandle::equals(const WebSpeechRecognitionHandle& other) const
{
    return (m_private.get() == other.m_private.get());
}

bool WebSpeechRecognitionHandle::lessThan(const WebSpeechRecognitionHandle& other) const
{
    return (m_private.get() < other.m_private.get());
}

WebSpeechRecognitionHandle::WebSpeechRecognitionHandle(const PassRefPtr<SpeechRecognition>& speechRecognition)
    : m_private(speechRecognition)
{
}

WebSpeechRecognitionHandle& WebSpeechRecognitionHandle::operator=(const PassRefPtr<SpeechRecognition>& speechRecognition)
{
    m_private = speechRecognition;
    return *this;
}

WebSpeechRecognitionHandle::operator PassRefPtr<SpeechRecognition>() const
{
    return m_private.get();
}

} // namespace WebKit
