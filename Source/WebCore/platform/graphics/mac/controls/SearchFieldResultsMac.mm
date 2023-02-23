/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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

#import "config.h"
#import "SearchFieldResultsMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "SearchFieldResultsPart.h"

namespace WebCore {

SearchFieldResultsMac::SearchFieldResultsMac(SearchFieldResultsPart& owningPart, ControlFactoryMac& controlFactory, NSSearchFieldCell *searchFieldCell, NSMenu *searchMenuTemplate)
    : SearchControlMac(owningPart, controlFactory, searchFieldCell)
    , m_searchMenuTemplate(searchMenuTemplate)
{
    ASSERT(owningPart.type() == StyleAppearance::SearchFieldResultsButton || owningPart.type() == StyleAppearance::SearchFieldResultsDecoration);
    ASSERT(searchFieldCell);
    ASSERT((owningPart.type() == StyleAppearance::SearchFieldResultsButton) == (searchMenuTemplate != nullptr));
}

void SearchFieldResultsMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    SearchControlMac::updateCellStates(rect, style);

    if ([m_searchFieldCell searchMenuTemplate] != m_searchMenuTemplate)
        [m_searchFieldCell setSearchMenuTemplate:m_searchMenuTemplate.get()];
}

void SearchFieldResultsMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto logicalRect = borderRect.rect();

    if (style.zoomFactor != 1) {
        logicalRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    drawCell(context, logicalRect, deviceScaleFactor, style, [m_searchFieldCell searchButtonCell], true);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
