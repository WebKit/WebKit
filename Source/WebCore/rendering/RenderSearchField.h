/*
 * Copyright (C) 2006, 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#pragma once

#include "PopupMenuClient.h"
#include "RenderTextControlSingleLine.h"
#include "SearchPopupMenu.h"

namespace WebCore {

class HTMLInputElement;

class RenderSearchField final : public RenderTextControlSingleLine, private PopupMenuClient {
    WTF_MAKE_ISO_ALLOCATED(RenderSearchField);
public:
    RenderSearchField(HTMLInputElement&, RenderStyle&&);
    virtual ~RenderSearchField();

    void updateCancelButtonVisibility() const;

    void addSearchResult();

    bool popupIsVisible() const { return m_searchPopupIsVisible; }
    void showPopup();
    void hidePopup();
    WEBCORE_EXPORT std::span<const RecentSearch> recentSearches();

private:
    void willBeDestroyed() override;
    LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
    void updateFromElement() override;
    Visibility visibilityForCancelButton() const;
    const AtomString& autosaveName() const;

    // PopupMenuClient methods
    void valueChanged(unsigned listIndex, bool fireEvents = true) override;
    void selectionChanged(unsigned, bool) override { }
    void selectionCleared() override { }
    String itemText(unsigned listIndex) const override;
    String itemLabel(unsigned listIndex) const override;
    String itemIcon(unsigned listIndex) const override;
    String itemToolTip(unsigned) const override { return String(); }
    String itemAccessibilityText(unsigned) const override { return String(); }
    bool itemIsEnabled(unsigned listIndex) const override;
    PopupMenuStyle itemStyle(unsigned listIndex) const override;
    PopupMenuStyle menuStyle() const override;
    int clientInsetLeft() const override;
    int clientInsetRight() const override;
    LayoutUnit clientPaddingLeft() const override;
    LayoutUnit clientPaddingRight() const override;
    int listSize() const override;
    int selectedIndex() const override;
    void popupDidHide() override;
    bool itemIsSeparator(unsigned listIndex) const override;
    bool itemIsLabel(unsigned listIndex) const override;
    bool itemIsSelected(unsigned listIndex) const override;
    bool shouldPopOver() const override { return false; }
    void setTextFromItem(unsigned listIndex) override;
    FontSelector* fontSelector() const override;
    HostWindow* hostWindow() const override;
    Ref<Scrollbar> createScrollbar(ScrollableArea&, ScrollbarOrientation, ScrollbarWidth) override;

    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;

    bool m_searchPopupIsVisible;
    RefPtr<SearchPopupMenu> m_searchPopup;
    Vector<RecentSearch> m_recentSearches;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSearchField, isRenderSearchField())
