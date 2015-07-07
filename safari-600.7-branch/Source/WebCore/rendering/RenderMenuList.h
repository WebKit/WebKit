/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef RenderMenuList_h
#define RenderMenuList_h

#include "LayoutRect.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "RenderFlexibleBox.h"

#if PLATFORM(COCOA)
#define POPUP_MENU_PULLS_DOWN 0
#else
#define POPUP_MENU_PULLS_DOWN 1
#endif

namespace WebCore {

class HTMLSelectElement;
class RenderText;

class RenderMenuList final : public RenderFlexibleBox, private PopupMenuClient {

public:
    RenderMenuList(HTMLSelectElement&, PassRef<RenderStyle>);
    virtual ~RenderMenuList();

    HTMLSelectElement& selectElement() const;

#if !PLATFORM(IOS)
    bool popupIsVisible() const { return m_popupIsVisible; }
#endif
    void showPopup();
    void hidePopup();

    void setOptionsChanged(bool changed) { m_needsOptionsWidthUpdate = changed; }

    void didSetSelectedIndex(int listIndex);

    String text() const;

private:
    void element() const = delete;

    virtual bool isMenuList() const override { return true; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;
    virtual RenderObject* removeChild(RenderObject&) override;
    virtual bool createsAnonymousWrapper() const override { return true; }

    virtual void updateFromElement() override;

    virtual LayoutRect controlClipRect(const LayoutPoint&) const override;
    virtual bool hasControlClip() const override { return true; }
    virtual bool canHaveGeneratedChildren() const override { return false; }

    virtual const char* renderName() const override { return "RenderMenuList"; }

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual bool requiresForcedStyleRecalcPropagation() const override { return true; }

    // PopupMenuClient methods
    virtual void valueChanged(unsigned listIndex, bool fireOnChange = true) override;
    virtual void selectionChanged(unsigned, bool) override { }
    virtual void selectionCleared() override { }
    virtual String itemText(unsigned listIndex) const override;
    virtual String itemLabel(unsigned listIndex) const override;
    virtual String itemIcon(unsigned listIndex) const override;
    virtual String itemToolTip(unsigned listIndex) const override;
    virtual String itemAccessibilityText(unsigned listIndex) const override;
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
    virtual bool shouldPopOver() const override { return !POPUP_MENU_PULLS_DOWN; }
    virtual bool valueShouldChangeOnHotTrack() const override { return true; }
    virtual void setTextFromItem(unsigned listIndex) override;
    virtual void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true) override;
    virtual bool multiple() const override;
    virtual FontSelector* fontSelector() const override;
    virtual HostWindow* hostWindow() const override;
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize) override;

    virtual bool hasLineIfEmpty() const override { return true; }

    // Flexbox defines baselines differently than regular blocks.
    // For backwards compatibility, menulists need to do the regular block behavior.
    virtual int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const override
    {
        return RenderBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    virtual int firstLineBaseline() const override { return RenderBlock::firstLineBaseline(); }
    virtual int inlineBlockBaseline(LineDirectionMode direction) const override { return RenderBlock::inlineBlockBaseline(direction); }

    void getItemBackgroundColor(unsigned listIndex, Color&, bool& itemHasCustomBackgroundColor) const;

    void createInnerBlock();
    void adjustInnerStyle();
    void setText(const String&);
    void setTextFromOption(int optionIndex);
    void updateOptionsWidth();

    void didUpdateActiveOption(int optionIndex);

    RenderText* m_buttonText;
    RenderBlock* m_innerBlock;

    bool m_needsOptionsWidthUpdate;
    int m_optionsWidth;

    int m_lastActiveIndex;

    RefPtr<RenderStyle> m_optionStyle;

#if !PLATFORM(IOS)
    RefPtr<PopupMenu> m_popup;
    bool m_popupIsVisible;
#endif
};

RENDER_OBJECT_TYPE_CASTS(RenderMenuList, isMenuList())

}

#endif
