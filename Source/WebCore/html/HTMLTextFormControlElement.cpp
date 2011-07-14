/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLTextFormControlElement.h"

#include "Attribute.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "RenderBox.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "ScriptEventListener.h"
#include "TextIterator.h"
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

HTMLTextFormControlElement::HTMLTextFormControlElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* form)
    : HTMLFormControlElementWithState(tagName, doc, form)
    , m_cachedSelectionStart(-1)
    , m_cachedSelectionEnd(-1)
{
}

HTMLTextFormControlElement::~HTMLTextFormControlElement()
{
}

void HTMLTextFormControlElement::insertedIntoDocument()
{
    HTMLFormControlElement::insertedIntoDocument();
    String initialValue = value();
    setTextAsOfLastFormControlChangeEvent(initialValue.isNull() ? emptyString() : initialValue);
}

void HTMLTextFormControlElement::dispatchFocusEvent()
{
    if (supportsPlaceholder())
        updatePlaceholderVisibility(false);
    handleFocusEvent();
    HTMLFormControlElementWithState::dispatchFocusEvent();
}

void HTMLTextFormControlElement::dispatchBlurEvent()
{
    if (supportsPlaceholder())
        updatePlaceholderVisibility(false);
    handleBlurEvent();
    HTMLFormControlElementWithState::dispatchBlurEvent();
}

void HTMLTextFormControlElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().webkitEditableContentChangedEvent && renderer() && renderer()->isTextControl()) {
        toRenderTextControl(renderer())->subtreeHasChanged();
        return;
    }

    HTMLFormControlElementWithState::defaultEventHandler(event);
}

String HTMLTextFormControlElement::strippedPlaceholder() const
{
    // According to the HTML5 specification, we need to remove CR and LF from
    // the attribute value.
    const AtomicString& attributeValue = getAttribute(placeholderAttr);
    if (!attributeValue.contains(newlineCharacter) && !attributeValue.contains(carriageReturn))
        return attributeValue;

    Vector<UChar> stripped;
    unsigned length = attributeValue.length();
    stripped.reserveCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        UChar character = attributeValue[i];
        if (character == newlineCharacter || character == carriageReturn)
            continue;
        stripped.append(character);
    }
    return String::adopt(stripped);
}

static bool isNotLineBreak(UChar ch) { return ch != newlineCharacter && ch != carriageReturn; }

bool HTMLTextFormControlElement::isPlaceholderEmpty() const
{
    const AtomicString& attributeValue = getAttribute(placeholderAttr);
    return attributeValue.string().find(isNotLineBreak) == notFound;
}

bool HTMLTextFormControlElement::placeholderShouldBeVisible() const
{
    return supportsPlaceholder()
        && isEmptyValue()
        && isEmptySuggestedValue()
        && !isPlaceholderEmpty()
        && (document()->focusedNode() != this || (renderer() && renderer()->theme()->shouldShowPlaceholderWhenFocused()));
}

void HTMLTextFormControlElement::updatePlaceholderVisibility(bool placeholderValueChanged)
{
    if (!supportsPlaceholder())
        return;
    if (!placeholderElement() || placeholderValueChanged)
        updatePlaceholderText();
    HTMLElement* placeholder = placeholderElement();
    if (!placeholder)
        return;
    ExceptionCode ec = 0;
    placeholder->getInlineStyleDecl()->setProperty(CSSPropertyVisibility, placeholderShouldBeVisible() ? "visible" : "hidden", ec);
    ASSERT(!ec);
}

RenderTextControl* HTMLTextFormControlElement::textRendererAfterUpdateLayout()
{
    if (!isTextFormControl())
        return 0;
    document()->updateLayoutIgnorePendingStylesheets();
    return toRenderTextControl(renderer());
}

void HTMLTextFormControlElement::setSelectionStart(int start)
{
    setSelectionRange(start, max(start, selectionEnd()));
}

void HTMLTextFormControlElement::setSelectionEnd(int end)
{
    setSelectionRange(min(end, selectionStart()), end);
}

void HTMLTextFormControlElement::select()
{
    setSelectionRange(0, numeric_limits<int>::max());
}

String HTMLTextFormControlElement::selectedText() const
{
    // FIXME: We should be able to extract selected contents even if there were no renderer.
    if (!renderer() || renderer()->isTextControl())
        return String();

    RenderTextControl* textControl = toRenderTextControl(renderer());
    return textControl->text().substring(selectionStart(), selectionEnd() - selectionStart());
}

void HTMLTextFormControlElement::dispatchFormControlChangeEvent()
{
    if (m_textAsOfLastFormControlChangeEvent != value()) {
        HTMLElement::dispatchChangeEvent();
        setTextAsOfLastFormControlChangeEvent(value());
    }
    setChangedSinceLastFormControlChangeEvent(false);
}

static inline bool hasVisibleTextArea(RenderTextControl* textControl, HTMLElement* innerText)
{
    ASSERT(textControl);
    return textControl->style()->visibility() != HIDDEN && innerText && innerText->renderer() && innerText->renderBox()->height();
}

void HTMLTextFormControlElement::setSelectionRange(int start, int end)
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer() || !renderer()->isTextControl())
        return;

    end = max(end, 0);
    start = min(max(start, 0), end);

    RenderTextControl* control = toRenderTextControl(renderer());
    if (!hasVisibleTextArea(control, innerTextElement())) {
        cacheSelection(start, end);
        return;
    }
    VisiblePosition startPosition = control->visiblePositionForIndex(start);
    VisiblePosition endPosition;
    if (start == end)
        endPosition = startPosition;
    else
        endPosition = control->visiblePositionForIndex(end);

    // startPosition and endPosition can be null position for example when
    // "-webkit-user-select: none" style attribute is specified.
    if (startPosition.isNotNull() && endPosition.isNotNull()) {
        ASSERT(startPosition.deepEquivalent().deprecatedNode()->shadowAncestorNode() == this
            && endPosition.deepEquivalent().deprecatedNode()->shadowAncestorNode() == this);
    }
    VisibleSelection newSelection = VisibleSelection(startPosition, endPosition);

    if (Frame* frame = document()->frame())
        frame->selection()->setSelection(newSelection);
}

int HTMLTextFormControlElement::indexForVisiblePosition(const VisiblePosition& pos) const
{
    Position indexPosition = pos.deepEquivalent().parentAnchoredEquivalent();
    if (enclosingTextFormControl(indexPosition) != this)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(indexPosition.document());
    range->setStart(innerTextElement(), 0, ec);
    ASSERT(!ec);
    range->setEnd(indexPosition.containerNode(), indexPosition.offsetInContainerNode(), ec);
    ASSERT(!ec);
    return TextIterator::rangeLength(range.get());
}

int HTMLTextFormControlElement::selectionStart() const
{
    if (!isTextFormControl())
        return 0;
    if (document()->focusedNode() != this && hasCachedSelectionStart())
        return m_cachedSelectionStart;

    return computeSelectionStart();
}

int HTMLTextFormControlElement::computeSelectionStart() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    return indexForVisiblePosition(frame->selection()->start());
}

int HTMLTextFormControlElement::selectionEnd() const
{
    if (!isTextFormControl())
        return 0;
    if (document()->focusedNode() != this && hasCachedSelectionEnd())
        return m_cachedSelectionEnd;
    return computeSelectionEnd();
}

int HTMLTextFormControlElement::computeSelectionEnd() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    return indexForVisiblePosition(frame->selection()->end());
}

static inline void setContainerAndOffsetForRange(Node* node, int offset, Node*& containerNode, int& offsetInContainer)
{
    if (node->isTextNode()) {
        containerNode = node;
        offsetInContainer = offset;
    } else {
        containerNode = node->parentNode();
        offsetInContainer = node->nodeIndex() + offset;
    }
}

PassRefPtr<Range> HTMLTextFormControlElement::selection() const
{
    if (!renderer() || !isTextFormControl() || !hasCachedSelectionStart() || !hasCachedSelectionEnd())
        return 0;

    int start = m_cachedSelectionStart;
    int end = m_cachedSelectionEnd;

    ASSERT(start <= end);
    HTMLElement* innerText = innerTextElement();
    if (!innerText)
        return 0;

    if (!innerText->firstChild())
        return Range::create(document(), innerText, 0, innerText, 0);

    int offset = 0;
    Node* startNode = 0;
    Node* endNode = 0;
    for (Node* node = innerText->firstChild(); node; node = node->traverseNextNode(innerText)) {
        ASSERT(!node->firstChild());
        ASSERT(node->isTextNode() || node->hasTagName(brTag));
        int length = node->isTextNode() ? lastOffsetInNode(node) : 1;

        if (offset <= start && start <= offset + length)
            setContainerAndOffsetForRange(node, start - offset, startNode, start);

        if (offset <= end && end <= offset + length) {
            setContainerAndOffsetForRange(node, end - offset, endNode, end);
            break;
        }

        offset += length;
    }

    if (!startNode || !endNode)
        return 0;

    return Range::create(document(), startNode, start, endNode, end);
}

void HTMLTextFormControlElement::restoreCachedSelection()
{
    setSelectionRange(m_cachedSelectionStart, m_cachedSelectionEnd);
}

void HTMLTextFormControlElement::selectionChanged(bool userTriggered)
{
    if (!renderer() || !isTextFormControl())
        return;

    // selectionStart() or selectionEnd() will return cached selection when this node doesn't have focus
    cacheSelection(computeSelectionStart(), computeSelectionEnd());

    if (Frame* frame = document()->frame()) {
        if (frame->selection()->isRange() && userTriggered)
            dispatchEvent(Event::create(eventNames().selectEvent, true, false));
    }
}

void HTMLTextFormControlElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == placeholderAttr)
        updatePlaceholderVisibility(true);
    else if (attr->name() == onselectAttr)
        setAttributeEventListener(eventNames().selectEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onchangeAttr)
        setAttributeEventListener(eventNames().changeEvent, createAttributeEventListener(this, attr));
    else
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
}

HTMLTextFormControlElement* enclosingTextFormControl(const Position& position)
{
    ASSERT(position.isNull() || position.anchorType() == Position::PositionIsOffsetInAnchor
           || position.containerNode() || !position.anchorNode()->shadowAncestorNode());
    Node* container = position.containerNode();
    if (!container)
        return 0;
    Node* ancestor = container->shadowAncestorNode();
    return ancestor != container ? toTextFormControl(ancestor) : 0;
}

} // namespace Webcore
