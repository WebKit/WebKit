/*
 * Copyright (c) 2013 Google Inc. All rights reserved.
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
#include "PlatformSpeechSynthesizer.h"

#include "PlatformSpeechSynthesisUtterance.h"
#include "PlatformSpeechSynthesisVoice.h"
#include "WebSpeechSynthesizerClientImpl.h"
#include <public/Platform.h>
#include <public/WebSpeechSynthesisUtterance.h>
#include <public/WebSpeechSynthesizer.h>
#include <public/WebSpeechSynthesizerClient.h>
#include <wtf/RetainPtr.h>

#if ENABLE(SPEECH_SYNTHESIS)

namespace WebCore {

PlatformSpeechSynthesizer::PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient* client)
    : m_speechSynthesizerClient(client)
{
    m_webSpeechSynthesizerClient = adoptPtr(new WebSpeechSynthesizerClientImpl(this, client));
    m_webSpeechSynthesizer = adoptPtr(WebKit::Platform::current()->createSpeechSynthesizer(m_webSpeechSynthesizerClient.get()));
}

PlatformSpeechSynthesizer::~PlatformSpeechSynthesizer()
{
}

void PlatformSpeechSynthesizer::initializeVoiceList()
{
    if (m_webSpeechSynthesizer.get())
        m_webSpeechSynthesizer->updateVoiceList();
}

void PlatformSpeechSynthesizer::speak(PassRefPtr<PlatformSpeechSynthesisUtterance> utterance)
{
    if (!m_webSpeechSynthesizer || !m_webSpeechSynthesizerClient)
        return;

    m_webSpeechSynthesizer->speak(WebKit::WebSpeechSynthesisUtterance(utterance));
}

void PlatformSpeechSynthesizer::pause()
{
    if (m_webSpeechSynthesizer.get())
        m_webSpeechSynthesizer->pause();
}

void PlatformSpeechSynthesizer::resume()
{
    if (m_webSpeechSynthesizer.get())
        m_webSpeechSynthesizer->resume();
}

void PlatformSpeechSynthesizer::cancel()
{
    if (m_webSpeechSynthesizer.get())
        m_webSpeechSynthesizer->cancel();
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
