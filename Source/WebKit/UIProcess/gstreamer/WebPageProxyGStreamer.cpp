/*
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPageProxy.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(SPEECH_SYNTHESIS)

#include "MessageSenderInlines.h"
#include "WebPageMessages.h"
#include "WebPageProxyInternals.h"

namespace WebKit {

void WebPageProxy::Internals::didStartSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (auto& handler = speechSynthesisData().speakingStartedCompletionHandler)
        handler();
}

void WebPageProxy::Internals::didFinishSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (auto& handler = speechSynthesisData().speakingFinishedCompletionHandler)
        handler();
}

void WebPageProxy::Internals::didPauseSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (auto& handler = speechSynthesisData().speakingPausedCompletionHandler)
        handler();
}

void WebPageProxy::Internals::didResumeSpeaking(WebCore::PlatformSpeechSynthesisUtterance&)
{
    if (auto& handler = speechSynthesisData().speakingResumedCompletionHandler)
        handler();
}

void WebPageProxy::Internals::speakingErrorOccurred(WebCore::PlatformSpeechSynthesisUtterance&)
{
    page.legacyMainFrameProcess().send(Messages::WebPage::SpeakingErrorOccurred(), page.webPageIDInMainFrameProcess());
}

void WebPageProxy::Internals::boundaryEventOccurred(WebCore::PlatformSpeechSynthesisUtterance&, WebCore::SpeechBoundary speechBoundary, unsigned charIndex, unsigned charLength)
{
    page.legacyMainFrameProcess().send(Messages::WebPage::BoundaryEventOccurred(speechBoundary == WebCore::SpeechBoundary::SpeechWordBoundary, charIndex, charLength), page.webPageIDInMainFrameProcess());
}

void WebPageProxy::Internals::voicesDidChange()
{
    page.legacyMainFrameProcess().send(Messages::WebPage::VoicesDidChange(), page.webPageIDInMainFrameProcess());
}

} // namespace WebKit

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(SPEECH_SYNTHESIS)
