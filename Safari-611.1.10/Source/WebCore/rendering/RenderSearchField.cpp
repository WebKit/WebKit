/**
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

#include "CSSFontSelector.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "Font.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PopupMenu.h"
#include "RenderLayer.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "StyleResolver.h"
#include "TextControlInnerElements.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSearchField);

RenderSearchField::RenderSearchField(HTMLInputElement& element, RenderStyle&& style)
    : RenderTextControlSingleLine(element, WTFMove(style))
    , m_searchPopupIsVisible(false)
    , m_searchPopup(0)
{
    ASSERT(element.isSearchField());
}

RenderSearchField::~RenderSearchField()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderSearchField::willBeDestroyed()
{
    if (m_searchPopup) {
        m_searchPopup->popupMenu()->disconnectClient();
        m_searchPopup = nullptr;
    }

    RenderTextControlSingleLine::willBeDestroyed();
}

inline HTMLElement* RenderSearchField::resultsButtonElement() const
{
    return inputElement().resultsButtonElement();
}

inline HTMLElement* RenderSearchField::cancelButtonElement() const
{
    return inputElement().cancelButtonElement();
}

void RenderSearchField::addSearchResult()
{
    if (inputElement().maxResults() <= 0)
        return;

    String value = inputElement().value();
    if (value.isEmpty())
        return;

    if (page().usesEphemeralSession())
        return;

    m_recentSearches.removeAllMatching([value] (const RecentSearch& recentSearch) {
        return recentSearch.string == value;
    });

    RecentSearch recentSearch = { value, WallTime::now() };
    m_recentSearches.insert(0, recentSearch);
    while (static_cast<int>(m_recentSearches.size()) > inputElement().maxResults())
        m_recentSearches.removeLast();

    const AtomString& name = autosaveName();
    if (!m_searchPopup)
        m_searchPopup = page().chrome().createSearchPopupMenu(*this);

    m_searchPopup->saveRecentSearches(name, m_recentSearches);
}

void RenderSearchField::showPopup()
{
    if (m_searchPopupIsVisible)
        return;

    if (!m_searchPopup)
        m_searchPopup = page().chrome().createSearchPopupMenu(*this);

    if (!m_searchPopup->enabled())
        return;

    m_searchPopupIsVisible = true;

    const AtomString& name = autosaveName();
    m_searchPopup->loadRecentSearches(name, m_recentSearches);

    // Trim the recent searches list if the maximum size has changed since we last saved.
    if (static_cast<int>(m_recentSearches.size()) > inputElement().maxResults()) {
        do {
            m_recentSearches.removeLast();
        } while (static_cast<int>(m_recentSearches.size()) > inputElement().maxResults());

        m_searchPopup->saveRecentSearches(name, m_recentSearches);
    }

    FloatPoint absTopLeft = localToAbsolute(FloatPoint(), UseTransforms);
    IntRect absBounds = absoluteBoundingBoxRectIgnoringTransforms();
    absBounds.setLocation(roundedIntPoint(absTopLeft));
    m_searchPopup->popupMenu()->show(absBounds, &view().frameView(), -1);
}

void RenderSearchField::hidePopup()
{
    if (m_searchPopup)
        m_searchPopup->popupMenu()->hide();
}

LayoutUnit RenderSearchField::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    HTMLElement* resultsButton = resultsButtonElement();
    if (RenderBox* resultsRenderer = resultsButton ? resultsButton->renderBox() : 0) {
        resultsRenderer->updateLogicalHeight();
        nonContentHeight = std::max(nonContentHeight, resultsRenderer->borderAndPaddingLogicalHeight() + resultsRenderer->marginLogicalHeight());
        lineHeight = std::max(lineHeight, resultsRenderer->logicalHeight());
    }
    HTMLElement* cancelButton = cancelButtonElement();
    if (RenderBox* cancelRenderer = cancelButton ? cancelButton->renderBox() : 0) {
        cancelRenderer->updateLogicalHeight();
        nonContentHeight = std::max(nonContentHeight, cancelRenderer->borderAndPaddingLogicalHeight() + cancelRenderer->marginLogicalHeight());
        lineHeight = std::max(lineHeight, cancelRenderer->logicalHeight());
    }

    return lineHeight + nonContentHeight;
}

void RenderSearchField::updateFromElement()
{
    RenderTextControlSingleLine::updateFromElement();

    if (cancelButtonElement())
        updateCancelButtonVisibility();

    if (m_searchPopupIsVisible)
        m_searchPopup->popupMenu()->updateFromElement();
}

void RenderSearchField::updateCancelButtonVisibility() const
{
    RenderElement* cancelButtonRenderer = cancelButtonElement()->renderer();
    if (!cancelButtonRenderer)
        return;

    const RenderStyle& curStyle = cancelButtonRenderer->style();
    Visibility buttonVisibility = visibilityForCancelButton();
    if (curStyle.visibility() == buttonVisibility)
        return;

    auto cancelButtonStyle = RenderStyle::clone(curStyle);
    cancelButtonStyle.setVisibility(buttonVisibility);
    cancelButtonRenderer->setStyle(WTFMove(cancelButtonStyle));
}

Visibility RenderSearchField::visibilityForCancelButton() const
{
    return (style().visibility() == Visibility::Hidden || inputElement().value().isEmpty()) ? Visibility::Hidden : Visibility::Visible;
}

const AtomString& RenderSearchField::autosaveName() const
{
    return inputElement().attributeWithoutSynchronization(autosaveAttr);
}

// PopupMenuClient methods
void RenderSearchField::valueChanged(unsigned listIndex, bool fireEvents)
{
    ASSERT(static_cast<int>(listIndex) < listSize());
    if (static_cast<int>(listIndex) == (listSize() - 1)) {
        if (fireEvents) {
            m_recentSearches.clear();
            const AtomString& name = autosaveName();
            if (!name.isEmpty()) {
                if (!m_searchPopup)
                    m_searchPopup = page().chrome().createSearchPopupMenu(*this);
                m_searchPopup->saveRecentSearches(name, m_recentSearches);
            }
        }
    } else {
        inputElement().setValue(itemText(listIndex));
        if (fireEvents)
            inputElement().onSearch();
        inputElement().select();
    }
}

String RenderSearchField::itemText(unsigned listIndex) const
{
#if !PLATFORM(IOS_FAMILY)
    int size = listSize();
    if (size == 1) {
        ASSERT(!listIndex);
        return searchMenuNoRecentSearchesText();
    }
    if (!listIndex)
        return searchMenuRecentSearchesText();
#endif
    if (itemIsSeparator(listIndex))
        return String();
#if !PLATFORM(IOS_FAMILY)
    if (static_cast<int>(listIndex) == (size - 1))
        return searchMenuClearRecentSearchesText();
#endif
    return m_recentSearches[listIndex - 1].string;
}

String RenderSearchField::itemLabel(unsigned) const
{
    return String();
}

String RenderSearchField::itemIcon(unsigned) const
{
    return String();
}

bool RenderSearchField::itemIsEnabled(unsigned listIndex) const
{
     if (!listIndex || itemIsSeparator(listIndex))
        return false;
    return true;
}

PopupMenuStyle RenderSearchField::itemStyle(unsigned) const
{
    return menuStyle();
}

PopupMenuStyle RenderSearchField::menuStyle() const
{
    return PopupMenuStyle(style().visitedDependentColorWithColorFilter(CSSPropertyColor), style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor), style().fontCascade(), style().visibility() == Visibility::Visible,
        style().display() == DisplayType::None, true, style().textIndent(), style().direction(), isOverride(style().unicodeBidi()), PopupMenuStyle::CustomBackgroundColor);
}

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
    if (RenderBox* box = innerBlockElement() ? innerBlockElement()->renderBox() : 0)
        padding += box->x();
    return padding;
}

LayoutUnit RenderSearchField::clientPaddingRight() const
{
    LayoutUnit padding = paddingRight();
    if (RenderBox* containerBox = containerElement() ? containerElement()->renderBox() : 0) {
        if (RenderBox* innerBlockBox = innerBlockElement() ? innerBlockElement()->renderBox() : 0)
            padding += containerBox->width() - (innerBlockBox->x() + innerBlockBox->width());
    }
    return padding;
}

int RenderSearchField::listSize() const
{
    // If there are no recent searches, then our menu will have 1 "No recent searches" item.
    if (!m_recentSearches.size())
        return 1;
    // Otherwise, leave room in the menu for a header, a separator, and the "Clear recent searches" item.
    return m_recentSearches.size() + 3;
}

int RenderSearchField::selectedIndex() const
{
    return -1;
}

void RenderSearchField::popupDidHide()
{
    m_searchPopupIsVisible = false;
}

bool RenderSearchField::itemIsSeparator(unsigned listIndex) const
{
    // The separator will be the second to last item in our list.
    return static_cast<int>(listIndex) == (listSize() - 2);
}

bool RenderSearchField::itemIsLabel(unsigned listIndex) const
{
    return !listIndex;
}

bool RenderSearchField::itemIsSelected(unsigned) const
{
    return false;
}

void RenderSearchField::setTextFromItem(unsigned listIndex)
{
    inputElement().setValue(itemText(listIndex));
}

FontSelector* RenderSearchField::fontSelector() const
{
    return &document().fontSelector();
}

HostWindow* RenderSearchField::hostWindow() const
{
    return view().frameView().hostWindow();
}

Ref<Scrollbar> RenderSearchField::createScrollbar(ScrollableArea& scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    bool hasCustomScrollbarStyle = style().hasPseudoStyle(PseudoId::Scrollbar);
    if (hasCustomScrollbarStyle)
        return RenderScrollbar::createCustomScrollbar(scrollableArea, orientation, &inputElement());
    return Scrollbar::createNativeScrollbar(scrollableArea, orientation, controlSize);
}

}
