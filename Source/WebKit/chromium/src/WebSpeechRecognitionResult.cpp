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
#include "WebSpeechRecognitionResult.h"

#include "SpeechRecognitionAlternative.h"
#include "SpeechRecognitionResult.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

void WebSpeechRecognitionResult::assign(const WebSpeechRecognitionResult& other)
{
    m_private = other.m_private;
}

void WebSpeechRecognitionResult::assign(const WebVector<WebString>& transcripts, const WebVector<float>& confidences, bool final)
{
    ASSERT(transcripts.size() == confidences.size());

    Vector<RefPtr<WebCore::SpeechRecognitionAlternative> > alternatives(transcripts.size());
    for (size_t i = 0; i < transcripts.size(); ++i)
        alternatives[i] = WebCore::SpeechRecognitionAlternative::create(transcripts[i], confidences[i]);

    m_private = WebCore::SpeechRecognitionResult::create(alternatives, final);
}

void WebSpeechRecognitionResult::reset()
{
    m_private.reset();
}

WebSpeechRecognitionResult::operator PassRefPtr<WebCore::SpeechRecognitionResult>() const
{
    return m_private.get();
}

} // namespace WebKit
