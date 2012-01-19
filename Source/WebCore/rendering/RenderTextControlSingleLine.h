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

#ifndef RenderTextControlSingleLine_h
#define RenderTextControlSingleLine_h

#include "PopupMenuClient.h"
#include "RenderTextControl.h"

namespace WebCore {

class HTMLInputElement;
class SearchPopupMenu;

class RenderTextControlSingleLine : public RenderTextControl, private PopupMenuClient {
public:
    RenderTextControlSingleLine(Node*);
    virtual ~RenderTextControlSingleLine();
    // FIXME: Move create*Style() to their classes.
    virtual PassRefPtr<RenderStyle> createInnerTextStyle(const RenderStyle* startStyle) const;
    PassRefPtr<RenderStyle> createInnerBlockStyle(const RenderStyle* startStyle) const;
    void updateCancelButtonVisibility() const;

    void addSearchResult();
    void stopSearchEventTimer();

    bool popupIsVisible() const { return m_searchPopupIsVisible; }
    void showPopup();
    void hidePopup();

    void capsLockStateMayHaveChanged();

#if ENABLE(INPUT_SPEECH)
    HTMLElement* speechButtonElement() const;
#endif

private:
    virtual bool hasControlClip() const;
    virtual LayoutRect controlClipRect(const LayoutPoint&) const;
    virtual bool isTextField() const { return true; }

    virtual void paint(PaintInfo&, const LayoutPoint&);
    virtual void layout();

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual void autoscroll();

    // Subclassed to forward to our inner div.
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Node** stopNode = 0);
    virtual bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier = 1, Node** stopNode = 0);

    int textBlockWidth() const;
    virtual float getAvgCharWidth(AtomicString family);
    virtual LayoutUnit preferredContentWidth(float charWidth) const;
    virtual void adjustControlHeightBasedOnLineHeight(LayoutUnit lineHeight);

    virtual void updateFromElement();
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual RenderStyle* textBaseStyle() const;

    EVisibility visibilityForCancelButton() const;
    bool textShouldBeTruncated() const;
    const AtomicString& autosaveName() const;

    // PopupMenuClient methods
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true);
    virtual void selectionChanged(unsigned, bool) {}
    virtual void selectionCleared() {}
    virtual String itemText(unsigned listIndex) const;
    virtual String itemLabel(unsigned listIndex) const;
    virtual String itemIcon(unsigned listIndex) const;
    virtual String itemToolTip(unsigned) const { return String(); }
    virtual String itemAccessibilityText(unsigned) const { return String(); }
    virtual bool itemIsEnabled(unsigned listIndex) const;
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const;
    virtual PopupMenuStyle menuStyle() const;
    virtual int clientInsetLeft() const;
    virtual int clientInsetRight() const;
    virtual int clientPaddingLeft() const;
    virtual int clientPaddingRight() const;
    virtual int listSize() const;
    virtual int selectedIndex() const;
    virtual void popupDidHide();
    virtual bool itemIsSeparator(unsigned listIndex) const;
    virtual bool itemIsLabel(unsigned listIndex) const;
    virtual bool itemIsSelected(unsigned listIndex) const;
    virtual bool shouldPopOver() const { return false; }
    virtual bool valueShouldChangeOnHotTrack() const { return false; }
    virtual void setTextFromItem(unsigned listIndex);
    virtual FontSelector* fontSelector() const;
    virtual HostWindow* hostWindow() const;
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize);

    HTMLInputElement* inputElement() const;

    HTMLElement* containerElement() const;
    HTMLElement* innerBlockElement() const;
    HTMLElement* innerSpinButtonElement() const;
    HTMLElement* resultsButtonElement() const;
    HTMLElement* cancelButtonElement() const;

    bool m_searchPopupIsVisible;
    bool m_shouldDrawCapsLockIndicator;
    LayoutUnit m_desiredInnerTextHeight;
    RefPtr<SearchPopupMenu> m_searchPopup;
    Vector<String> m_recentSearches;
};

inline RenderTextControlSingleLine* toRenderTextControlSingleLine(RenderObject* object)
{
    ASSERT(!object || object->isTextField());
    return static_cast<RenderTextControlSingleLine*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderTextControlSingleLine(const RenderTextControlSingleLine*);

// ----------------------------

class RenderTextControlInnerBlock : public RenderBlock {
public:
    RenderTextControlInnerBlock(Node* node, bool isMultiLine) : RenderBlock(node), m_multiLine(isMultiLine) { }

private:
    virtual bool hasLineIfEmpty() const { return true; }
    virtual VisiblePosition positionForPoint(const LayoutPoint&);

    bool m_multiLine;
};

}

#endif
