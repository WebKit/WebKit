/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/PlatformSpeechSynthesisUtterance.h>
#include <WebCore/PlatformSpeechSynthesisVoice.h>
#include <WebCore/SpeechSynthesisClient.h>

namespace WebKit {

class WebPage;
    
class WebSpeechSynthesisClient : public WebCore::SpeechSynthesisClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebSpeechSynthesisClient(WebPage& page)
        : m_page(page)
    {
    }
    
    virtual ~WebSpeechSynthesisClient() { }
    
    const Vector<RefPtr<WebCore::PlatformSpeechSynthesisVoice>>& voiceList() override;
    void speak(RefPtr<WebCore::PlatformSpeechSynthesisUtterance>) override;
    void cancel() override;
    void pause() override;
    void resume() override;
private:
    void setObserver(WeakPtr<WebCore::SpeechSynthesisClientObserver> observer) override { m_observer = observer; }
    WeakPtr<WebCore::SpeechSynthesisClientObserver> observer() const override { return m_observer; }

    WebCore::SpeechSynthesisClientObserver* corePageObserver() const;
    
    WebPage& m_page;
    WeakPtr<WebCore::SpeechSynthesisClientObserver> m_observer;
    Vector<RefPtr<WebCore::PlatformSpeechSynthesisVoice>> m_voices;
};

} // namespace WebKit

#endif // ENABLE(SPEECH_SYNTHESIS)
