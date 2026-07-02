/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FindIndicator.h"

#include "MessageSenderInlines.h"
#include "PluginView.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/FindRevealAlgorithms.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FindIndicator);

bool FindIndicator::update(WebCore::LocalFrame* selectedFrame, bool isShowingOverlay, bool shouldAnimate)
{
    OptionSet<TextIndicatorOption> textIndicatorOptions { TextIndicatorOption::IncludeMarginIfRangeMatchesSelection };
    auto presentationTransition = shouldAnimate ? TextIndicatorPresentationTransition::Bounce : TextIndicatorPresentationTransition::None;

    auto [frame, indicator] = [&]() -> std::tuple<RefPtr<Frame>, RefPtr<TextIndicator>> {
        RefPtr webPage { m_webPage.get() };
#if ENABLE(PDF_PLUGIN)
        if (RefPtr pluginView = protect(m_webPage.get())->mainFramePlugIn())
            return { webPage->mainFrame(), pluginView->textIndicatorForCurrentSelection(textIndicatorOptions, presentationTransition) };
#endif
        if (!selectedFrame)
            return { };

        auto selectedRange = selectedFrame->selection().selection().toNormalizedRange();
        if (!selectedRange)
            return { };

        if (ImageOverlay::isInsideOverlay(*selectedRange))
            textIndicatorOptions.add({ TextIndicatorOption::PaintAllContent, TextIndicatorOption::PaintBackgrounds });

        if (selectedRange->collapsed())
            selectedRange = selectedFrame->selection().selection().range();

        return { selectedFrame, TextIndicator::createWithRange(*selectedRange, textIndicatorOptions, presentationTransition) };
    }();

    if (!indicator)
        return false;

    m_findIndicatorRect = enclosingIntRect(indicator->selectionRectInRootViewCoordinates());
#if PLATFORM(COCOA)
    m_webPage->send(Messages::WebPageProxy::SetTextIndicatorFromFrame(frame->frameID(), WTF::move(indicator), isShowingOverlay ? WebCore::TextIndicatorLifetime::Permanent : WebCore::TextIndicatorLifetime::Temporary));
#endif
    m_isShowingFindIndicator = true;

    return true;
}

void FindIndicator::hide()
{
    if (!m_isShowingFindIndicator)
        return;

    protect(m_webPage.get())->send(Messages::WebPageProxy::ClearTextIndicator());
    m_isShowingFindIndicator = false;
}

void FindIndicator::didFindString(WebCore::LocalFrame* selectedFrame)
{
    if (!selectedFrame)
        return;

    CheckedRef selection = selectedFrame->selection();
    selection->revealSelection();
    if (RefPtr anchorNode = selection->selection().start().anchorNode())
        revealClosedDetailsAndHiddenUntilFoundAncestors(*anchorNode);
}

}
