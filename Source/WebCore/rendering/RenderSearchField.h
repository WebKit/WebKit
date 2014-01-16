/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderSearchField_h
#define RenderSearchField_h

#include "PopupMenuClient.h"
#include "RenderTextControlSingleLine.h"

namespace WebCore {

class HTMLInputElement;
class SearchPopupMenu;

class RenderSearchField final : public RenderTextControlSingleLine, private PopupMenuClient {
public:
    RenderSearchField(HTMLInputElement&, PassRef<RenderStyle>);
    virtual ~RenderSearchField();

    void updateCancelButtonVisibility() const;

    void addSearchResult();
    void stopSearchEventTimer();

    bool popupIsVisible() const { return m_searchPopupIsVisible; }
    void showPopup();
    void hidePopup();

private:
    virtual void centerContainerIfNeeded(RenderBox*) const override;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
    virtual LayoutUnit computeLogicalHeightLimit() const override;
    virtual void updateFromElement() override;
    EVisibility visibilityForCancelButton() const;
    const AtomicString& autosaveName() const;

    // PopupMenuClient methods
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true) override;
    virtual void selectionChanged(unsigned, bool) override { }
    virtual void selectionCleared() override { }
    virtual String itemText(unsigned listIndex) const override;
    virtual String itemLabel(unsigned listIndex) const override;
    virtual String itemIcon(unsigned listIndex) const override;
    virtual String itemToolTip(unsigned) const override { return String(); }
    virtual String itemAccessibilityText(unsigned) const override { return String(); }
    virtual bool itemIsEnabled(unsigned listIndex) const override;
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const override;
    virtual PopupMenuStyle menuStyle() const override;
    virtual int clientInsetLeft() const override;
    virtual int clientInsetRight() const override;
    virtual LayoutUnit clientPaddingLeft() const override;
    virtual LayoutUnit clientPaddingRight() const override;
    virtual int listSize() const override;
    virtual int selectedIndex() const override;
    virtual void popupDidHide() override;
    virtual bool itemIsSeparator(unsigned listIndex) const override;
    virtual bool itemIsLabel(unsigned listIndex) const override;
    virtual bool itemIsSelected(unsigned listIndex) const override;
    virtual bool shouldPopOver() const override { return false; }
    virtual bool valueShouldChangeOnHotTrack() const override { return false; }
    virtual void setTextFromItem(unsigned listIndex) override;
    virtual FontSelector* fontSelector() const override;
    virtual HostWindow* hostWindow() const override;
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize) override;

    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;

    bool m_searchPopupIsVisible;
    RefPtr<SearchPopupMenu> m_searchPopup;
    Vector<String> m_recentSearches;
};

RENDER_OBJECT_TYPE_CASTS(RenderSearchField, isTextField())

}

#endif
