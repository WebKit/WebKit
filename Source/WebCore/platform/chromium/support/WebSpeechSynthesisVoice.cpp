/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include <public/WebSpeechSynthesisVoice.h>

#include "SpeechRecognitionAlternative.h"
#include "SpeechSynthesisVoice.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

#if ENABLE(SPEECH_SYNTHESIS)

namespace WebKit {

void WebSpeechSynthesisVoice::assign(const WebSpeechSynthesisVoice& other)
{
    m_private = other.m_private;
}

void WebSpeechSynthesisVoice::reset()
{
    m_private.reset();
}

void WebSpeechSynthesisVoice::setVoiceURI(const WebString& voiceURI)
{
    m_private->setVoiceURI(voiceURI);
}

void WebSpeechSynthesisVoice::setName(const WebString& name)
{
    m_private->setName(name);
}

void WebSpeechSynthesisVoice::setLanguage(const WebString& language)
{
    m_private->setLang(language);
}

void WebSpeechSynthesisVoice::setIsLocalService(bool isLocalService)
{
    m_private->setLocalService(isLocalService);
}

void WebSpeechSynthesisVoice::setIsDefault(bool isDefault)
{
    m_private->setIsDefault(isDefault);
}

WebSpeechSynthesisVoice::operator PassRefPtr<WebCore::PlatformSpeechSynthesisVoice>() const
{
    return m_private.get();
}

} // namespace WebKit

#endif // ENABLE(SPEECH_SYNTHESIS)
