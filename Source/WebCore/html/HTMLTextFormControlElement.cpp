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

#include "AXObjectCache.h"
#include "Attribute.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "ScriptEventListener.h"
#include "Text.h"
#include "TextIterator.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

HTMLTextFormControlElement::HTMLTextFormControlElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* form)
    : HTMLFormControlElementWithState(tagName, doc, form)
    , m_lastChangeWasUserEdit(false)
    , m_cachedSelectionStart(-1)
    , m_cachedSelectionEnd(-1)
    , m_cachedSelectionDirection(SelectionHasNoDirection)
{
}

HTMLTextFormControlElement::~HTMLTextFormControlElement()
{
}

bool HTMLTextFormControlElement::childShouldCreateRenderer(const NodeRenderingContext& childContext) const
{
    return childContext.isOnEncapsulationBoundary() && HTMLFormControlElementWithState::childShouldCreateRenderer(childContext);
}

Node::InsertionNotificationRequest HTMLTextFormControlElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLFormControlElement::insertedInto(insertionPoint);
    if (!insertionPoint->inDocument())
        return InsertionDone;
    String initialValue = value();
    setTextAsOfLastFormControlChangeEvent(initialValue.isNull() ? emptyString() : initialValue);
    return InsertionDone;
}

void HTMLTextFormControlElement::dispatchFocusEvent(PassRefPtr<Node> oldFocusedNode)
{
    if (supportsPlaceholder())
        updatePlaceholderVisibility(false);
    handleFocusEvent();
    HTMLFormControlElementWithState::dispatchFocusEvent(oldFocusedNode);
}

void HTMLTextFormControlElement::dispatchBlurEvent(PassRefPtr<Node> newFocusedNode)
{
    if (supportsPlaceholder())
        updatePlaceholderVisibility(false);
    handleBlurEvent();
    HTMLFormControlElementWithState::dispatchBlurEvent(newFocusedNode);
}

void HTMLTextFormControlElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().webkitEditableContentChangedEvent && renderer() && renderer()->isTextControl()) {
        m_lastChangeWasUserEdit = true;
        subtreeHasChanged();
        return;
    }

    HTMLFormControlElementWithState::defaultEventHandler(event);
}

void HTMLTextFormControlElement::forwardEvent(Event* event)
{
    if (event->type() == eventNames().blurEvent || event->type() == eventNames().focusEvent)
        return;
    innerTextElement()->defaultEventHandler(event);
}

String HTMLTextFormControlElement::strippedPlaceholder() const
{
    // According to the HTML5 specification, we need to remove CR and LF from
    // the attribute value.
    const AtomicString& attributeValue = getAttribute(placeholderAttr);
    if (!attributeValue.contains(newlineCharacter) && !attributeValue.contains(carriageReturn))
        return attributeValue;

    StringBuilder stripped;
    unsigned length = attributeValue.length();
    stripped.reserveCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        UChar character = attributeValue[i];
        if (character == newlineCharacter || character == carriageReturn)
            continue;
        stripped.append(character);
    }
    return stripped.toString();
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
        && (document()->focusedNode() != this || (renderer() && renderer()->theme()->shouldShowPlaceholderWhenFocused()))
        && (!renderer() || renderer()->style()->visibility() == VISIBLE);
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
    placeholder->setInlineStyleProperty(CSSPropertyVisibility, placeholderShouldBeVisible() ? "visible" : "hidden", ec);
    ASSERT(!ec);
}

void HTMLTextFormControlElement::fixPlaceholderRenderer(HTMLElement* placeholder, HTMLElement* siblingElement)
{
    // FIXME: We should change the order of DOM nodes. But it makes an assertion
    // failure in editing code.
    if (!placeholder || !placeholder->renderer())
        return;
    RenderObject* placeholderRenderer = placeholder->renderer();
    RenderObject* siblingRenderer = siblingElement->renderer();
    ASSERT(siblingRenderer);
    if (placeholderRenderer->nextSibling() == siblingRenderer)
        return;
    RenderObject* parentRenderer = placeholderRenderer->parent();
    ASSERT(parentRenderer == siblingRenderer->parent());
    parentRenderer->removeChild(placeholderRenderer);
    parentRenderer->addChild(placeholderRenderer, siblingRenderer);
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
    setSelectionRange(start, max(start, selectionEnd()), selectionDirection());
}

void HTMLTextFormControlElement::setSelectionEnd(int end)
{
    setSelectionRange(min(end, selectionStart()), end, selectionDirection());
}

void HTMLTextFormControlElement::setSelectionDirection(const String& direction)
{
    setSelectionRange(selectionStart(), selectionEnd(), direction);
}

void HTMLTextFormControlElement::select()
{
    setSelectionRange(0, numeric_limits<int>::max(), SelectionHasNoDirection);
}

String HTMLTextFormControlElement::selectedText() const
{
    if (!isTextFormControl())
        return String();
    return value().substring(selectionStart(), selectionEnd() - selectionStart());
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

void HTMLTextFormControlElement::setSelectionRange(int start, int end, const String& directionString)
{
    TextFieldSelectionDirection direction = SelectionHasNoDirection;
    if (directionString == "forward")
        direction = SelectionHasForwardDirection;
    else if (directionString == "backward")
        direction = SelectionHasBackwardDirection;

    return setSelectionRange(start, end, direction);
}

void HTMLTextFormControlElement::setSelectionRange(int start, int end, TextFieldSelectionDirection direction)
{
    document()->updateLayoutIgnorePendingStylesheets();

    if (!renderer() || !renderer()->isTextControl())
        return;

    end = max(end, 0);
    start = min(max(start, 0), end);

    RenderTextControl* control = toRenderTextControl(renderer());
    if (!hasVisibleTextArea(control, innerTextElement())) {
        cacheSelection(start, end, direction);
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
    VisibleSelection newSelection;
    if (direction == SelectionHasBackwardDirection)
        newSelection = VisibleSelection(endPosition, startPosition);
    else
        newSelection = VisibleSelection(startPosition, endPosition);
    newSelection.setIsDirectional(direction != SelectionHasNoDirection);

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
    if (document()->focusedNode() != this && hasCachedSelection())
        return m_cachedSelectionStart;

    return computeSelectionStart();
}

int HTMLTextFormControlElement::computeSelectionStart() const
{
    ASSERT(isTextFormControl());
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    return indexForVisiblePosition(frame->selection()->start());
}

int HTMLTextFormControlElement::selectionEnd() const
{
    if (!isTextFormControl())
        return 0;
    if (document()->focusedNode() != this && hasCachedSelection())
        return m_cachedSelectionEnd;
    return computeSelectionEnd();
}

int HTMLTextFormControlElement::computeSelectionEnd() const
{
    ASSERT(isTextFormControl());
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    return indexForVisiblePosition(frame->selection()->end());
}

static const AtomicString& directionString(TextFieldSelectionDirection direction)
{
    DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
    DEFINE_STATIC_LOCAL(const AtomicString, forward, ("forward"));
    DEFINE_STATIC_LOCAL(const AtomicString, backward, ("backward"));

    switch (direction) {
    case SelectionHasNoDirection:
        return none;
    case SelectionHasForwardDirection:
        return forward;
    case SelectionHasBackwardDirection:
        return backward;
    }

    ASSERT_NOT_REACHED();
    return none;
}

const AtomicString& HTMLTextFormControlElement::selectionDirection() const
{
    if (!isTextFormControl())
        return directionString(SelectionHasNoDirection);
    if (document()->focusedNode() != this && hasCachedSelection())
        return directionString(m_cachedSelectionDirection);

    return directionString(computeSelectionDirection());
}

TextFieldSelectionDirection HTMLTextFormControlElement::computeSelectionDirection() const
{
    ASSERT(isTextFormControl());
    Frame* frame = document()->frame();
    if (!frame)
        return SelectionHasNoDirection;

    const VisibleSelection& selection = frame->selection()->selection();
    return selection.isDirectional() ? (selection.isBaseFirst() ? SelectionHasForwardDirection : SelectionHasBackwardDirection) : SelectionHasNoDirection;
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
    if (!renderer() || !isTextFormControl() || !hasCachedSelection())
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
    setSelectionRange(m_cachedSelectionStart, m_cachedSelectionEnd, m_cachedSelectionDirection);
}

void HTMLTextFormControlElement::selectionChanged(bool userTriggered)
{
    if (!renderer() || !isTextFormControl())
        return;

    // selectionStart() or selectionEnd() will return cached selection when this node doesn't have focus
    cacheSelection(computeSelectionStart(), computeSelectionEnd(), computeSelectionDirection());

    if (Frame* frame = document()->frame()) {
        if (frame->selection()->isRange() && userTriggered)
            dispatchEvent(Event::create(eventNames().selectEvent, true, false));
    }
}

void HTMLTextFormControlElement::parseAttribute(const Attribute& attribute)
{
    if (attribute.name() == placeholderAttr)
        updatePlaceholderVisibility(true);
    else if (attribute.name() == onselectAttr)
        setAttributeEventListener(eventNames().selectEvent, createAttributeEventListener(this, attribute));
    else if (attribute.name() == onchangeAttr)
        setAttributeEventListener(eventNames().changeEvent, createAttributeEventListener(this, attribute));
    else
        HTMLFormControlElementWithState::parseAttribute(attribute);
}

void HTMLTextFormControlElement::notifyFormStateChanged()
{
    Frame* frame = document()->frame();
    if (!frame)
        return;

    if (Page* page = frame->page())
        page->chrome()->client()->formStateDidChange(this);
}

bool HTMLTextFormControlElement::lastChangeWasUserEdit() const
{
    if (!isTextFormControl())
        return false;
    return m_lastChangeWasUserEdit;
}

void HTMLTextFormControlElement::setInnerTextValue(const String& value)
{
    if (!isTextFormControl())
        return;

    bool textIsChanged = value != innerTextValue();
    if (textIsChanged || !innerTextElement()->hasChildNodes()) {
        if (textIsChanged && document() && renderer() && AXObjectCache::accessibilityEnabled())
            document()->axObjectCache()->postNotification(this, AXObjectCache::AXValueChanged, false);

        ExceptionCode ec = 0;
        innerTextElement()->setInnerText(value, ec);
        ASSERT(!ec);

        if (value.endsWith('\n') || value.endsWith('\r')) {
            innerTextElement()->appendChild(HTMLBRElement::create(document()), ec);
            ASSERT(!ec);
        }
    }

    setFormControlValueMatchesRenderer(true);
}

static String finishText(StringBuilder& result)
{
    // Remove one trailing newline; there's always one that's collapsed out by rendering.
    size_t size = result.length();
    if (size && result[size - 1] == '\n')
        result.resize(--size);
    return result.toString();
}

String HTMLTextFormControlElement::innerTextValue() const
{
    HTMLElement* innerText = innerTextElement();
    if (!innerText || !isTextFormControl())
        return emptyString();

    StringBuilder result;
    for (Node* node = innerText; node; node = node->traverseNextNode(innerText)) {
        if (node->hasTagName(brTag))
            result.append(newlineCharacter);
        else if (node->isTextNode())
            result.append(toText(node)->data());
    }
    return finishText(result);
}

static void getNextSoftBreak(RootInlineBox*& line, Node*& breakNode, unsigned& breakOffset)
{
    RootInlineBox* next;
    for (; line; line = next) {
        next = line->nextRootBox();
        if (next && !line->endsWithBreak()) {
            ASSERT(line->lineBreakObj());
            breakNode = line->lineBreakObj()->node();
            breakOffset = line->lineBreakPos();
            line = next;
            return;
        }
    }
    breakNode = 0;
    breakOffset = 0;
}

String HTMLTextFormControlElement::valueWithHardLineBreaks() const
{
    // FIXME: It's not acceptable to ignore the HardWrap setting when there is no renderer.
    // While we have no evidence this has ever been a practical problem, it would be best to fix it some day.
    HTMLElement* innerText = innerTextElement();
    if (!innerText || !isTextFormControl())
        return value();

    RenderBlock* renderer = toRenderBlock(innerText->renderer());
    if (!renderer)
        return value();

    Node* breakNode;
    unsigned breakOffset;
    RootInlineBox* line = renderer->firstRootBox();
    if (!line)
        return value();

    getNextSoftBreak(line, breakNode, breakOffset);

    StringBuilder result;
    for (Node* node = innerText->firstChild(); node; node = node->traverseNextNode(innerText)) {
        if (node->hasTagName(brTag))
            result.append(newlineCharacter);
        else if (node->isTextNode()) {
            String data = toText(node)->data();
            unsigned length = data.length();
            unsigned position = 0;
            while (breakNode == node && breakOffset <= length) {
                if (breakOffset > position) {
                    result.append(data.characters() + position, breakOffset - position);
                    position = breakOffset;
                    result.append(newlineCharacter);
                }
                getNextSoftBreak(line, breakNode, breakOffset);
            }
            result.append(data.characters() + position, length - position);
        }
        while (breakNode == node)
            getNextSoftBreak(line, breakNode, breakOffset);
    }
    return finishText(result);
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

static const Element* parentHTMLElement(const Element* element)
{
    while (element) {
        element = element->parentElement();
        if (element && element->isHTMLElement())
            return element;
    }
    return 0;
}

String HTMLTextFormControlElement::directionForFormData() const
{
    for (const Element* element = this; element; element = parentHTMLElement(element)) {
        const AtomicString& dirAttributeValue = element->fastGetAttribute(dirAttr);
        if (dirAttributeValue.isNull())
            continue;

        if (equalIgnoringCase(dirAttributeValue, "rtl") || equalIgnoringCase(dirAttributeValue, "ltr"))
            return dirAttributeValue;

        if (equalIgnoringCase(dirAttributeValue, "auto")) {
            bool isAuto;
            TextDirection textDirection = static_cast<const HTMLElement*>(element)->directionalityIfhasDirAutoAttribute(isAuto);
            return textDirection == RTL ? "rtl" : "ltr";
        }
    }

    return "ltr";
}

} // namespace Webcore
