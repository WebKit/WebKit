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

#include "ContainerNodeInlines.h"
#include "CSSFontSelector.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "Font.h"
#include "FrameSelection.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "NodeInlines.h"
#include "Page.h"
#include "PopupMenu.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SearchInputType.h"
#include "StyleResolver.h"
#include "TextControlInnerElements.h"
#include "UnicodeBidi.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSearchField);

RenderSearchField::RenderSearchField(HTMLInputElement& element, RenderStyle&& style)
    : RenderTextControlSingleLine(Type::SearchField, element, WTF::move(style))
    , m_searchPopupIsVisible(false)
    , m_searchPopup(nullptr)
{
    ASSERT(element.isSearchField());
    ASSERT(isRenderSearchField());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderSearchField::~RenderSearchField() = default;

void RenderSearchField::willBeDestroyed()
{
    if (RefPtr searchPopup = std::exchange(m_searchPopup, nullptr))
        searchPopup->protectedPopupMenu()->disconnectClient();

    RenderTextControlSingleLine::willBeDestroyed();
}

inline HTMLElement* RenderSearchField::resultsButtonElement() const
{
    return protectedInputElement()->resultsButtonElement();
}

inline HTMLElement* RenderSearchField::cancelButtonElement() const
{
    return protectedInputElement()->cancelButtonElement();
}

void RenderSearchField::showPopup()
{
    if (m_searchPopupIsVisible)
        return;



    if (!m_searchPopup)
        m_searchPopup = page().chrome().createSearchPopupMenu(downcast<SearchInputType>(*protectedInputElement()->inputType()));

    Ref popup = *m_searchPopup;
    if (!popup->enabled())
        return;

    m_searchPopupIsVisible = true;

    auto recentSearches = downcast<SearchInputType>(*protectedInputElement()->inputType()).recentSearches();
    const AtomString& name = autosaveName();
    popup->loadRecentSearches(name, recentSearches);

    // Trim the recent searches list if the maximum size has changed since we last saved.

    if (static_cast<int>(recentSearches.size()) > protectedInputElement()->maxResults()) {
        do {
            recentSearches.removeLast();
        } while (static_cast<int>(recentSearches.size()) > protectedInputElement()->maxResults());

        popup->saveRecentSearches(name, recentSearches);
    }

    FloatPoint absTopLeft = localToAbsolute(FloatPoint(), UseTransforms);
    IntRect absBounds = absoluteBoundingBoxRectIgnoringTransforms();
    absBounds.setLocation(roundedIntPoint(absTopLeft));
    protectedSearchPopup()->protectedPopupMenu()->show(absBounds, view().frameView(), -1);
}

void RenderSearchField::hidePopup()
{
    if (RefPtr searchPopup = m_searchPopup)
        searchPopup->protectedPopupMenu()->hide();
}

LayoutUnit RenderSearchField::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    RefPtr resultsButton = resultsButtonElement();
    if (CheckedPtr resultsRenderer = resultsButton ? resultsButton->renderBox() : nullptr) {
        resultsRenderer->updateLogicalHeight();
        nonContentHeight = std::max(nonContentHeight, resultsRenderer->borderAndPaddingLogicalHeight() + resultsRenderer->marginLogicalHeight());
        lineHeight = std::max(lineHeight, resultsRenderer->logicalHeight());
    }
    RefPtr cancelButton = cancelButtonElement();
    if (CheckedPtr cancelRenderer = cancelButton ? cancelButton->renderBox() : nullptr) {
        cancelRenderer->updateLogicalHeight();
        nonContentHeight = std::max(nonContentHeight, cancelRenderer->borderAndPaddingLogicalHeight() + cancelRenderer->marginLogicalHeight());
        lineHeight = std::max(lineHeight, cancelRenderer->logicalHeight());
    }

    return lineHeight + nonContentHeight;
}
    
std::span<const RecentSearch> RenderSearchField::recentSearches()
{
    if (!m_searchPopup)
        m_searchPopup = page().chrome().createSearchPopupMenu(downcast<SearchInputType>(*protectedInputElement()->inputType()));

    auto& recentSearches = downcast<SearchInputType>(*protectedInputElement()->inputType()).recentSearches();

    const AtomString& name = autosaveName();
    protectedSearchPopup()->loadRecentSearches(name, recentSearches);

    return recentSearches.span();
}

void RenderSearchField::updateFromElement()
{
    RenderTextControlSingleLine::updateFromElement();

    if (cancelButtonElement())
        updateCancelButtonVisibility();

    if (m_searchPopupIsVisible)
        protectedSearchPopup()->protectedPopupMenu()->updateFromElement();
}

void RenderSearchField::updateCancelButtonVisibility() const
{
    CheckedPtr cancelButtonRenderer = cancelButtonElement()->renderer();
    if (!cancelButtonRenderer)
        return;

    CheckedRef curStyle = cancelButtonRenderer->style();
    Visibility buttonVisibility = visibilityForCancelButton();
    if (curStyle->usedVisibility() == buttonVisibility)
        return;

    auto cancelButtonStyle = RenderStyle::clone(curStyle.get());
    cancelButtonStyle.setVisibility(buttonVisibility);
    cancelButtonRenderer->setStyle(WTF::move(cancelButtonStyle));
}

Visibility RenderSearchField::visibilityForCancelButton() const
{
    return (style().usedVisibility() == Visibility::Hidden || protectedInputElement()->value()->isEmpty()) ? Visibility::Hidden : Visibility::Visible;
}

const AtomString& RenderSearchField::autosaveName() const
{
    return protectedInputElement()->attributeWithoutSynchronization(nameAttr);
}

void RenderSearchField::updatePopup(const AtomString& name, const Vector<RecentSearch>& searchItems)
{
    if (!m_searchPopup)
        m_searchPopup = page().chrome().createSearchPopupMenu(downcast<SearchInputType>(*protectedInputElement()->inputType()));
    protectedSearchPopup()->saveRecentSearches(name, searchItems);
}

void RenderSearchField::popupDidHide()
{
    m_searchPopupIsVisible = false;
}

#if PLATFORM(WIN)
int RenderSearchField::clientInsetLeft() const
{
    // Inset the menu by the radius of the cap on the left so that
    // it only runs along the straight part of the bezel.
    return height() / 2;
}

int RenderSearchField::clientInsetRight() const
{
    // Inset the menu by the radius of the cap on the right so that
    // it only runs along the straight part of the bezel (unless it needs
    // to be wider).
    return height() / 2;
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
            padding += containerBox->width() - (innerBlockBox->x() + innerBlockBox->width());
    }
    return padding;
}

FontSelector* RenderSearchField::fontSelector() const
{
    return &protect(document())->fontSelector();
}

HostWindow* RenderSearchField::hostWindow() const
{
    return RenderTextControlSingleLine::hostWindow();
}
#endif

}
