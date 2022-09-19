/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "WebFindOptions.h"
#include "WebFoundTextRange.h"
#include <WebCore/FindOptions.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIndicator.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {
class Document;
}

namespace WebKit {

class WebPage;

class WebFoundTextRangeController : private WebCore::PageOverlay::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebFoundTextRangeController);

public:
    explicit WebFoundTextRangeController(WebPage&);

    void findTextRangesForStringMatches(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(Vector<WebKit::WebFoundTextRange>&&)>&&);

    void replaceFoundTextRangeWithString(const WebFoundTextRange&, const String&);

    void decorateTextRangeWithStyle(const WebFoundTextRange&, FindDecorationStyle);
    void scrollTextRangeToVisible(const WebFoundTextRange&);

    void clearAllDecoratedFoundText();

    void didBeginTextSearchOperation();
    void didEndTextSearchOperation();

    void addLayerForFindOverlay(CompletionHandler<void(WebCore::GraphicsLayer::PlatformLayerID)>&&);
    void removeLayerForFindOverlay();

    void requestRectForFoundTextRange(const WebFoundTextRange&, CompletionHandler<void(WebCore::FloatRect)>&&);

    void redraw();

private:
    // PageOverlay::Client.
    void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override;
    void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;

    RefPtr<WebCore::TextIndicator> createTextIndicatorForRange(const WebCore::SimpleRange&, WebCore::TextIndicatorPresentationTransition);
    void setTextIndicatorWithRange(const WebCore::SimpleRange&);
    void flashTextIndicatorAndUpdateSelectionWithRange(const WebCore::SimpleRange&);

    Vector<WebCore::FloatRect> rectsForTextMatchesInRect(WebCore::IntRect clipRect);

    WebCore::Document* documentForFoundTextRange(const WebFoundTextRange&) const;
    std::optional<WebCore::SimpleRange> simpleRangeFromFoundTextRange(WebFoundTextRange);

    WeakPtr<WebPage> m_webPage;
    RefPtr<WebCore::PageOverlay> m_findPageOverlay;

    WebFoundTextRange m_highlightedRange;

    HashMap<WebFoundTextRange, std::optional<WebCore::SimpleRange>> m_cachedFoundRanges;
    HashMap<WebFoundTextRange, FindDecorationStyle> m_decoratedRanges;

    RefPtr<WebCore::TextIndicator> m_textIndicator;
};

} // namespace WebKit
