/*
 * Copyright (C) 2006, 2007, 2010, 2015 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderSearchField.h"

#include "ElementInlines.h"
#include "HTMLInputElement.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSearchField);

RenderSearchField::RenderSearchField(HTMLInputElement& element, Style::ComputedStyle&& style)
    : RenderTextControlSingleLine(Type::SearchField, element, WTF::move(style))
{
    ASSERT(element.isSearchField());
    ASSERT(isRenderSearchField());
}

RenderSearchField::~RenderSearchField() = default;

LayoutUnit RenderSearchField::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    RefPtr resultsButton = protect(inputElement())->resultsButtonElement();
    if (CheckedPtr resultsRenderer = resultsButton ? resultsButton->renderBox() : nullptr) {
        resultsRenderer->updateLogicalHeight();
        nonContentHeight = std::max(nonContentHeight, resultsRenderer->borderAndPaddingLogicalHeight() + resultsRenderer->marginLogicalHeight());
        lineHeight = std::max(lineHeight, resultsRenderer->logicalHeight());
    }
    RefPtr cancelButton = protect(inputElement())->cancelButtonElement();
    if (CheckedPtr cancelRenderer = cancelButton ? cancelButton->renderBox() : nullptr) {
        cancelRenderer->updateLogicalHeight();
        nonContentHeight = std::max(nonContentHeight, cancelRenderer->borderAndPaddingLogicalHeight() + cancelRenderer->marginLogicalHeight());
        lineHeight = std::max(lineHeight, cancelRenderer->logicalHeight());
    }

    return lineHeight + nonContentHeight;
}

#if PLATFORM(WIN)
int RenderSearchField::clientInsetLeft() const
{
    // Inset the menu by the radius of the cap on the left so that
    // it only runs along the straight part of the bezel.
    return borderBoxHeight() / 2;
}

int RenderSearchField::clientInsetRight() const
{
    // Inset the menu by the radius of the cap on the right so that
    // it only runs along the straight part of the bezel (unless it needs
    // to be wider).
    return borderBoxHeight() / 2;
}

LayoutUnit RenderSearchField::clientPaddingLeft() const
{
    LayoutUnit padding = paddingLeft();
    RefPtr innerBlock = innerBlockElement();
    if (auto* box = innerBlock ? innerBlock->renderBox() : nullptr)
        padding += box->x();
    return padding;
}

LayoutUnit RenderSearchField::clientPaddingRight() const
{
    LayoutUnit padding = paddingRight();
    RefPtr container = containerElement();
    if (CheckedPtr containerBox = container ? container->renderBox() : nullptr) {
        RefPtr innerBlock = innerBlockElement();
        if (auto* innerBlockBox = innerBlock ? innerBlock->renderBox() : nullptr)
            padding += containerBox->borderBoxWidth() - (innerBlockBox->x() + innerBlockBox->borderBoxWidth());
    }
    return padding;
}
#endif

} // namespace WebCore
