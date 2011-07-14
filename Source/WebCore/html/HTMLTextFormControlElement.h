/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLTextFormControlElement_h
#define HTMLTextFormControlElement_h

#include "HTMLFormControlElement.h"

namespace WebCore {

class Position;
class RenderTextControl;
class VisiblePosition;

class HTMLTextFormControlElement : public HTMLFormControlElementWithState {
public:
    // Common flag for HTMLInputElement::tooLong() and HTMLTextAreaElement::tooLong().
    enum NeedsToCheckDirtyFlag {CheckDirtyFlag, IgnoreDirtyFlag};

    virtual ~HTMLTextFormControlElement();

    void forwardEvent(Event*);

    virtual void insertedIntoDocument();

    // The derived class should return true if placeholder processing is needed.
    virtual bool supportsPlaceholder() const = 0;
    String strippedPlaceholder() const;
    bool placeholderShouldBeVisible() const;
    virtual HTMLElement* placeholderElement() const = 0;

    int indexForVisiblePosition(const VisiblePosition&) const;
    int selectionStart() const;
    int selectionEnd() const;
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void select();
    void setSelectionRange(int start, int end);
    PassRefPtr<Range> selection() const;
    String selectedText() const;

    virtual void dispatchFormControlChangeEvent();

    virtual int maxLength() const = 0;
    virtual String value() const = 0;

    virtual HTMLElement* innerTextElement() const = 0;

    void cacheSelection(int start, int end)
    {
        m_cachedSelectionStart = start;
        m_cachedSelectionEnd = end;
    }

    void selectionChanged(bool userTriggered);

protected:
    HTMLTextFormControlElement(const QualifiedName&, Document*, HTMLFormElement*);
    void updatePlaceholderVisibility(bool);
    virtual void updatePlaceholderText() = 0;

    virtual void parseMappedAttribute(Attribute*);

    void setTextAsOfLastFormControlChangeEvent(const String& text) { m_textAsOfLastFormControlChangeEvent = text; }

    void restoreCachedSelection();
    bool hasCachedSelectionStart() const { return m_cachedSelectionStart >= 0; }
    bool hasCachedSelectionEnd() const { return m_cachedSelectionEnd >= 0; }

    virtual void defaultEventHandler(Event*);
    virtual void subtreeHasChanged();

private:
    int computeSelectionStart() const;
    int computeSelectionEnd() const;

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    bool isPlaceholderEmpty() const;

    // Returns true if user-editable value is empty. Used to check placeholder visibility.
    virtual bool isEmptyValue() const = 0;
    // Returns true if suggested value is empty. Used to check placeholder visibility.
    virtual bool isEmptySuggestedValue() const { return true; }
    // Called in dispatchFocusEvent(), after placeholder process, before calling parent's dispatchFocusEvent().
    virtual void handleFocusEvent() { }
    // Called in dispatchBlurEvent(), after placeholder process, before calling parent's dispatchBlurEvent().
    virtual void handleBlurEvent() { }

    RenderTextControl* textRendererAfterUpdateLayout();

    String m_textAsOfLastFormControlChangeEvent;
    
    int m_cachedSelectionStart;
    int m_cachedSelectionEnd;
};

// This function returns 0 when node is an input element and not a text field.
inline HTMLTextFormControlElement* toTextFormControl(Node* node)
{
    return (node && node->isElementNode() && static_cast<Element*>(node)->isTextFormControl()) ? static_cast<HTMLTextFormControlElement*>(node) : 0;
}

HTMLTextFormControlElement* enclosingTextFormControl(const Position&);

} // namespace

#endif
