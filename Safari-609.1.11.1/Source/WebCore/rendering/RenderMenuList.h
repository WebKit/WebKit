/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2015 Apple Inc. All rights reserved.
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
    WTF_MAKE_ISO_ALLOCATED(RenderMenuList);
public:
    RenderMenuList(HTMLSelectElement&, RenderStyle&&);
    virtual ~RenderMenuList();

    HTMLSelectElement& selectElement() const;

#if !PLATFORM(IOS_FAMILY)
    bool popupIsVisible() const { return m_popupIsVisible; }
#endif
    void showPopup();
    void hidePopup();

    void setOptionsChanged(bool changed) { m_needsOptionsWidthUpdate = changed; }

    void didSetSelectedIndex(int listIndex);

    String text() const;

    RenderBlock* innerRenderer() const { return m_innerBlock.get(); }
    void setInnerRenderer(RenderBlock&);

    void didAttachChild(RenderObject& child, RenderObject* beforeChild);

private:
    void willBeDestroyed() override;

    void element() const = delete;

    bool isMenuList() const override { return true; }

    bool createsAnonymousWrapper() const override { return true; }

    void updateFromElement() override;

    LayoutRect controlClipRect(const LayoutPoint&) const override;
    bool hasControlClip() const override { return true; }
    bool canHaveGeneratedChildren() const override { return false; }

    const char* renderName() const override { return "RenderMenuList"; }

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    // PopupMenuClient methods
    void valueChanged(unsigned listIndex, bool fireOnChange = true) override;
    void selectionChanged(unsigned, bool) override { }
    void selectionCleared() override { }
    String itemText(unsigned listIndex) const override;
    String itemLabel(unsigned listIndex) const override;
    String itemIcon(unsigned listIndex) const override;
    String itemToolTip(unsigned listIndex) const override;
    String itemAccessibilityText(unsigned listIndex) const override;
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
    bool shouldPopOver() const override { return !POPUP_MENU_PULLS_DOWN; }
    bool valueShouldChangeOnHotTrack() const override { return true; }
    void setTextFromItem(unsigned listIndex) override;
    void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true) override;
    bool multiple() const override;
    FontSelector* fontSelector() const override;
    HostWindow* hostWindow() const override;
    Ref<Scrollbar> createScrollbar(ScrollableArea&, ScrollbarOrientation, ScrollbarControlSize) override;

    bool hasLineIfEmpty() const override { return true; }

    // Flexbox defines baselines differently than regular blocks.
    // For backwards compatibility, menulists need to do the regular block behavior.
    int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const override
    {
        return RenderBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    Optional<int> firstLineBaseline() const override { return RenderBlock::firstLineBaseline(); }
    Optional<int> inlineBlockBaseline(LineDirectionMode direction) const override { return RenderBlock::inlineBlockBaseline(direction); }

    void getItemBackgroundColor(unsigned listIndex, Color&, bool& itemHasCustomBackgroundColor) const;

    void adjustInnerStyle();
    void setText(const String&);
    void setTextFromOption(int optionIndex);
    void updateOptionsWidth();

    void didUpdateActiveOption(int optionIndex);

    bool isFlexibleBoxImpl() const override { return true; }

    WeakPtr<RenderText> m_buttonText;
    WeakPtr<RenderBlock> m_innerBlock;

    bool m_needsOptionsWidthUpdate;
    int m_optionsWidth;

    Optional<int> m_lastActiveIndex;

    std::unique_ptr<RenderStyle> m_optionStyle;

#if !PLATFORM(IOS_FAMILY)
    RefPtr<PopupMenu> m_popup;
    bool m_popupIsVisible;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMenuList, isMenuList())
