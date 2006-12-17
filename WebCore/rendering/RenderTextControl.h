/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RenderTextControl_h
#define RenderTextControl_h

#include "PopupMenuClient.h"
#include "RenderBlock.h"

namespace WebCore {

class HTMLTextFieldInnerElement;
class HTMLTextFieldInnerTextElement;
class HTMLSearchFieldCancelButtonElement;
class HTMLSearchFieldResultsButtonElement;
class SearchPopupMenu;

class RenderTextControl : public RenderBlock, public PopupMenuClient {
public:
    RenderTextControl(Node*, bool multiLine);
    virtual ~RenderTextControl();

    virtual const char* renderName() const { return "RenderTextControl"; }

    virtual void calcHeight();
    virtual void calcMinMaxWidth();
    virtual void removeLeftoverAnonymousBoxes() { }
    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();
    virtual bool canHaveChildren() const { return false; }
    virtual short baselinePosition(bool firstLine, bool isRootLineBox) const;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual void layout();
    virtual bool avoidsFloats() const { return true; }

    bool isEdited() const { return m_dirty; };
    void setEdited(bool isEdited) { m_dirty = isEdited; };
    bool isTextField() const { return !m_multiLine; }
    bool isTextArea() const { return m_multiLine; }

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void select();
    void setSelectionRange(int start, int end);

    void subtreeHasChanged();
    String text();
    String textWithHardLineBreaks();
    void forwardEvent(Event*);
    void selectionChanged(bool userTriggered);

    // Subclassed to forward to our inner div.
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);

    VisiblePosition visiblePositionForIndex(int index);
    int indexForVisiblePosition(const VisiblePosition&);

    void addSearchResult();
    void onSearch() const;

    bool popupIsVisible() const { return m_searchPopupIsVisible; }
    void showPopup();
    void hidePopup();

    // PopupMenuClient methods
    void valueChanged(unsigned listIndex, bool fireOnSearch = true);
    String itemText(unsigned listIndex) const;
    bool itemIsEnabled(unsigned listIndex) const;
    RenderStyle* itemStyle(unsigned listIndex) const;
    RenderStyle* clientStyle() const;
    Document* clientDocument() const;
    int clientPaddingLeft() const;
    int clientPaddingRight() const;
    unsigned listSize() const;
    int selectedIndex() const;
    bool itemIsSeparator(unsigned listIndex) const;
    bool itemIsLabel(unsigned listIndex) const;
    bool itemIsSelected(unsigned listIndex) const;
    void setTextFromItem(unsigned listIndex);
    bool shouldPopOver() const { return false; }
    bool valueShouldChangeOnHotTrack() const { return false; }

private:
    RenderStyle* createInnerBlockStyle(RenderStyle* startStyle);
    RenderStyle* createInnerTextStyle(RenderStyle* startStyle);
    RenderStyle* createCancelButtonStyle(RenderStyle* startStyle);
    RenderStyle* createResultsButtonStyle(RenderStyle* startStyle);

    void showPlaceholderIfNeeded();
    void hidePlaceholderIfNeeded();
    void createSubtreeIfNeeded();
    void updateCancelButtonVisibility(RenderStyle*);
    const AtomicString& autosaveName() const;

    RefPtr<HTMLTextFieldInnerElement> m_innerBlock;
    RefPtr<HTMLTextFieldInnerTextElement> m_innerText;
    RefPtr<HTMLSearchFieldResultsButtonElement> m_resultsButton;
    RefPtr<HTMLSearchFieldCancelButtonElement> m_cancelButton;

    bool m_dirty;
    bool m_multiLine;
    bool m_placeholderVisible;

    RefPtr<SearchPopupMenu> m_searchPopup;
    bool m_searchPopupIsVisible;
    mutable Vector<String> m_recentSearches;
};

} // namespace WebCore

#endif // RenderTextControl_h
