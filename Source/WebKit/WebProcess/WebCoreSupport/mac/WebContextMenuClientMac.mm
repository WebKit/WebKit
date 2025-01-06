/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebContextMenuClient.h"

#if ENABLE(CONTEXT_MENUS)

#import "MessageSenderInlines.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editor.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TranslationContextMenuInfo.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void WebContextMenuClient::lookUpInDictionary(LocalFrame* frame)
{
    m_page->performDictionaryLookupForSelection(*frame, frame->selection().selection(), TextIndicatorPresentationTransition::BounceAndCrossfade);
}

bool WebContextMenuClient::isSpeaking() const
{
    return m_page->isSpeaking();
}

void WebContextMenuClient::speak(const String&)
{
}

void WebContextMenuClient::stopSpeaking()
{
}

void WebContextMenuClient::searchWithGoogle(const LocalFrame* frame)
{
    auto searchString = frame->editor().selectedText().trim(deprecatedIsSpaceOrNewline);
    m_page->send(Messages::WebPageProxy::SearchTheWeb(searchString));
}

#if HAVE(TRANSLATION_UI_SERVICES)

void WebContextMenuClient::handleTranslation(const WebCore::TranslationContextMenuInfo& info)
{
    m_page->send(Messages::WebPageProxy::HandleContextMenuTranslation(info));
}

#endif // HAVE(TRANSLATION_UI_SERVICES)

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
