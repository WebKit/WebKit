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

namespace WebCore {

class HTMLSelectElement;
class RenderText;

class RenderMenuList final : public RenderFlexibleBox {
    WTF_MAKE_TZONE_ALLOCATED(RenderMenuList);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderMenuList);
public:
    RenderMenuList(HTMLSelectElement&, RenderStyle&&);
    virtual ~RenderMenuList();

    HTMLSelectElement& selectElement() const;

    // CheckedPtr interface.
    uint32_t checkedPtrCount() const { return RenderFlexibleBox::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const { return RenderFlexibleBox::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const { RenderFlexibleBox::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const { RenderFlexibleBox::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

#if !PLATFORM(IOS_FAMILY)
    bool popupIsVisible() const { return m_popupIsVisible; }
#endif
    void showPopup();
    void hidePopup();
    void popupDidHide();

    void setOptionsChanged(bool changed) { m_needsOptionsWidthUpdate = changed; }

    void didSetSelectedIndex(int listIndex);

    String text() const;

#if PLATFORM(IOS_FAMILY)
    void layout() override;
#endif

    RenderBlock* innerRenderer() const { return m_innerBlock.get(); }
    void setInnerRenderer(RenderBlock&);

    void didAttachChild(RenderObject& child, RenderObject* beforeChild);

    void getItemBackgroundColor(unsigned listIndex, Color&, bool& itemHasCustomBackgroundColor) const;

    PopupMenuStyle::Size popupMenuSize(const RenderStyle&);

    LayoutUnit clientPaddingLeft() const;
    LayoutUnit clientPaddingRight() const;

    HostWindow* hostWindow() const;
    void setTextFromOption(int optionIndex);

private:
    void willBeDestroyed() override;

    void element() const = delete;

    bool createsAnonymousWrapper() const override { return true; }

    void updateFromElement() override;

    LayoutRect controlClipRect(const LayoutPoint&) const override;
    bool hasControlClip() const override { return true; }
    bool canHaveGeneratedChildren() const override { return false; }

    ASCIILiteral renderName() const override { return "RenderMenuList"_s; }

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;

    void styleDidChange(Style::Difference, const RenderStyle* oldStyle) override;

    bool hasLineIfEmpty() const override { return true; }

    std::optional<LayoutUnit> firstLineBaseline() const override { return RenderBlock::firstLineBaseline(); }

    void adjustInnerStyle();
    void setText(const String&);
    void updateOptionsWidth();

    void didUpdateActiveOption(int optionIndex);

    bool isFlexibleBoxImpl() const override { return true; }

    SingleThreadWeakPtr<RenderText> m_buttonText;
    SingleThreadWeakPtr<RenderBlock> m_innerBlock;

    bool m_needsOptionsWidthUpdate;
    int m_optionsWidth;

    std::optional<int> m_lastActiveIndex;

    std::unique_ptr<RenderStyle> m_optionStyle;

#if !PLATFORM(IOS_FAMILY)
    RefPtr<PopupMenu> m_popup;
    bool m_popupIsVisible;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMenuList, isRenderMenuList())
