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

#include "config.h"
#include "WebFoundTextRangeController.h"

#include "WebPage.h"
#include <WebCore/CharacterRange.h>
#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIterator.h>

namespace WebKit {

WebFoundTextRangeController::WebFoundTextRangeController(WebPage& webPage)
    : m_webPage(webPage)
{
}

void WebFoundTextRangeController::findTextRangesForStringMatches(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& completionHandler)
{
    auto result = m_webPage->corePage()->findTextMatches(string, core(options), maxMatchCount, false);
    Vector<WebCore::SimpleRange> findMatches = WTFMove(result.ranges);

    String frameName;
    uint64_t order = 0;
    Vector<WebFoundTextRange> foundTextRanges;
    for (auto& simpleRange : findMatches) {
        auto& document = simpleRange.startContainer().document();

        auto* element = document.documentElement();
        if (!element)
            return;

        String currentFrameName = document.frame()->tree().uniqueName();
        if (frameName != currentFrameName) {
            frameName = currentFrameName;
            order++;
        }

        // FIXME: We should get the character ranges at the same time as the SimpleRanges to avoid additional traversals.
        auto range = characterRange(makeBoundaryPointBeforeNodeContents(*element), simpleRange, WebCore::findIteratorOptions());
        auto foundTextRange = WebFoundTextRange { range.location, range.length, frameName.length() ? frameName : emptyString(), order };

        foundTextRanges.append(foundTextRange);
    }

    completionHandler(WTFMove(foundTextRanges));
}

void WebFoundTextRangeController::decorateTextRangeWithStyle(const WebFoundTextRange& range, FindDecorationStyle style)
{
    UNUSED_PARAM(range);
    UNUSED_PARAM(style);
}

void WebFoundTextRangeController::scrollTextRangeToVisible(const WebFoundTextRange& range)
{
    UNUSED_PARAM(range);
}

void WebFoundTextRangeController::clearAllDecoratedFoundText()
{

}

void WebFoundTextRangeController::didBeginTextSearchOperation()
{

}

void WebFoundTextRangeController::didEndTextSearchOperation()
{

}

void WebFoundTextRangeController::willMoveToPage(WebCore::PageOverlay&, WebCore::Page* page)
{
    UNUSED_PARAM(page);
}

void WebFoundTextRangeController::didMoveToPage(WebCore::PageOverlay&, WebCore::Page*)
{

}

bool WebFoundTextRangeController::mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&)
{
    return false;
}

void WebFoundTextRangeController::drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    UNUSED_PARAM(graphicsContext);
    UNUSED_PARAM(dirtyRect);
}

} // namespace WebKit
