/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(SPEECH_SYNTHESIS)

#include <wtf/text/WTFString.h>

namespace WebCore {

class PlatformSpeechSynthesisVoice : public RefCounted<PlatformSpeechSynthesisVoice> {
public:
    NODELETE WEBCORE_EXPORT static Ref<PlatformSpeechSynthesisVoice> create(StringImpl* voiceURI, StringImpl* name, StringImpl* lang, bool localService, bool isDefault);
    NODELETE static Ref<PlatformSpeechSynthesisVoice> create();

    const RefPtr<StringImpl>& voiceURI() const LIFETIME_BOUND { return m_voiceURI; }
    void setVoiceURI(StringImpl* voiceURI) { m_voiceURI = voiceURI; }

    const RefPtr<StringImpl>& name() const LIFETIME_BOUND { return m_name; }
    void setName(StringImpl* name) { m_name = name; }

    const RefPtr<StringImpl>& lang() const LIFETIME_BOUND { return m_lang; }
    void setLang(StringImpl* lang) { m_lang = lang; }

    bool localService() const { return m_localService; }
    void setLocalService(bool localService) { m_localService = localService; }

    bool isDefault() const { return m_default; }
    void setIsDefault(bool isDefault) { m_default = isDefault; }

private:
    PlatformSpeechSynthesisVoice(StringImpl* voiceURI, StringImpl* name, StringImpl* lang, bool localService, bool isDefault);
    PlatformSpeechSynthesisVoice() = default;

    RefPtr<StringImpl> m_voiceURI;
    RefPtr<StringImpl> m_name;
    RefPtr<StringImpl> m_lang;
    bool m_localService { false };
    bool m_default { false };
};

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
