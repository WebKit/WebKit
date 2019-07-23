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

#include "config.h"
#include "WebSpeechSynthesisClient.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebSpeechSynthesisVoice.h"

#if ENABLE(SPEECH_SYNTHESIS)

namespace WebKit {

const Vector<RefPtr<WebCore::PlatformSpeechSynthesisVoice>>& WebSpeechSynthesisClient::voiceList()
{
    // FIXME: this message should not be sent synchronously. Instead, the UI process should
    // get the list of voices and pass it on to the WebContent processes, see
    // https://bugs.webkit.org/show_bug.cgi?id=195723
    Vector<WebSpeechSynthesisVoice> voiceList;
    m_page.sendSync(Messages::WebPageProxy::SpeechSynthesisVoiceList(), voiceList);

    m_voices.clear();
    for (auto& voice : voiceList)
        m_voices.append(WebCore::PlatformSpeechSynthesisVoice::create(voice.voiceURI, voice.name, voice.lang, voice.localService, voice.defaultLang));
    return m_voices;
}

WebCore::SpeechSynthesisClientObserver* WebSpeechSynthesisClient::corePageObserver() const
{
    if (m_page.corePage() && m_page.corePage()->speechSynthesisClient() && m_page.corePage()->speechSynthesisClient()->observer())
        return m_page.corePage()->speechSynthesisClient()->observer().get();
    return nullptr;
}

void WebSpeechSynthesisClient::speak(RefPtr<WebCore::PlatformSpeechSynthesisUtterance> utterance)
{
    WTF::CompletionHandler<void()> startedCompletionHandler = [this, weakThis = makeWeakPtr(*this)]() mutable {
        if (!weakThis)
            return;
        if (auto observer = corePageObserver())
            observer->didStartSpeaking();
    };

    WTF::CompletionHandler<void()> finishedCompletionHandler = [this, weakThis = makeWeakPtr(*this)]() mutable {
        if (!weakThis)
            return;
        if (auto observer = corePageObserver())
            observer->didFinishSpeaking();
    };

    auto voice = utterance->voice();
    auto voiceURI = voice ? voice->voiceURI() : "";
    auto name = voice ? voice->name() : "";
    auto lang = voice ? voice->lang() : "";
    auto localService = voice ? voice->localService() : false;
    auto isDefault = voice ? voice->isDefault() : false;
    
    m_page.sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisSetFinishedCallback(), WTFMove(finishedCompletionHandler));
    m_page.sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisSpeak(utterance->text(), utterance->lang(), utterance->volume(), utterance->rate(), utterance->pitch(), utterance->startTime(), voiceURI, name, lang, localService, isDefault), WTFMove(startedCompletionHandler));
}

void WebSpeechSynthesisClient::cancel()
{
    m_page.send(Messages::WebPageProxy::SpeechSynthesisCancel());
}

void WebSpeechSynthesisClient::pause()
{
    WTF::CompletionHandler<void()> completionHandler = [this, weakThis = makeWeakPtr(*this)]() mutable {
        if (!weakThis)
            return;
        if (auto observer = corePageObserver())
            observer->didPauseSpeaking();
    };
    
    m_page.sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisPause(), WTFMove(completionHandler));
}

void WebSpeechSynthesisClient::resume()
{
    WTF::CompletionHandler<void()> completionHandler = [this, weakThis = makeWeakPtr(*this)]() mutable {
        if (!weakThis)
            return;
        if (auto observer = corePageObserver())
            observer->didResumeSpeaking();
    };
    
    m_page.sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisResume(), WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(SPEECH_SYNTHESIS)
