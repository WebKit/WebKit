/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include "MessageSenderInlines.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebSpeechSynthesisVoice.h"
#include <WebCore/Page.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(SPEECH_SYNTHESIS)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSpeechSynthesisClient);

WebSpeechSynthesisClient::WebSpeechSynthesisClient(WebPage& page)
    : m_page(page)
{
}

const Vector<RefPtr<WebCore::PlatformSpeechSynthesisVoice>>& WebSpeechSynthesisClient::voiceList()
{
    RefPtr page = m_page.get();
    if (!page) {
        m_voices = { };
        return m_voices;
    }

    // FIXME: this message should not be sent synchronously. Instead, the UI process should
    // get the list of voices and pass it on to the WebContent processes, see
    // https://bugs.webkit.org/show_bug.cgi?id=195723
    auto sendResult = page->sendSync(Messages::WebPageProxy::SpeechSynthesisVoiceList());
    auto [voiceList] = sendResult.takeReplyOr(Vector<WebSpeechSynthesisVoice> { });

    m_voices = voiceList.map([](auto& voice) -> RefPtr<WebCore::PlatformSpeechSynthesisVoice> {
        return WebCore::PlatformSpeechSynthesisVoice::create(voice.voiceURI, voice.name, voice.lang, voice.localService, voice.defaultLang);
    });
    return m_voices;
}

WebCore::SpeechSynthesisClientObserver* WebSpeechSynthesisClient::corePageObserver() const
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    RefPtr corePage = page->corePage();
    if (corePage && corePage->speechSynthesisClient() && corePage->speechSynthesisClient()->observer())
        return corePage->speechSynthesisClient()->observer().get();
    return nullptr;
}

void WebSpeechSynthesisClient::resetState()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SpeechSynthesisResetState());
}

void WebSpeechSynthesisClient::speak(RefPtr<WebCore::PlatformSpeechSynthesisUtterance> utterance)
{
    CompletionHandler<void()> startedCompletionHandler = [weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr observer = protectedThis->corePageObserver())
            observer->didStartSpeaking();
    };

    CompletionHandler<void()> finishedCompletionHandler = [weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr observer = protectedThis->corePageObserver())
            observer->didFinishSpeaking();
    };

    RefPtr voice = utterance->voice();
    auto voiceURI = voice ? voice->voiceURI() : emptyString();
    auto name = voice ? voice->name() : emptyString();
    auto lang = voice ? voice->lang() : emptyString();
    auto localService = voice ? voice->localService() : false;
    auto isDefault = voice ? voice->isDefault() : false;

    RefPtr page = m_page.get();
    if (!page)
        return;

    page->sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisSetFinishedCallback(), WTFMove(finishedCompletionHandler));
    page->sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisSpeak(utterance->text(), utterance->lang(), utterance->volume(), utterance->rate(), utterance->pitch(), utterance->startTime(), voiceURI, name, lang, localService, isDefault), WTFMove(startedCompletionHandler));
}

void WebSpeechSynthesisClient::cancel()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SpeechSynthesisCancel());
}

void WebSpeechSynthesisClient::pause()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    CompletionHandler<void()> completionHandler = [weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr observer = protectedThis->corePageObserver())
            observer->didPauseSpeaking();
    };

    page->sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisPause(), WTFMove(completionHandler));
}

void WebSpeechSynthesisClient::resume()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    CompletionHandler<void()> completionHandler = [weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (RefPtr observer = protectedThis->corePageObserver())
            observer->didResumeSpeaking();
    };

    page->sendWithAsyncReply(Messages::WebPageProxy::SpeechSynthesisResume(), WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(SPEECH_SYNTHESIS)
