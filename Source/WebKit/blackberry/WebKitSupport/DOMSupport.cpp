/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DOMSupport.h"

#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "Node.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"

#include "htmlediting.h"
#include "visible_units.h"

#include <limits>

#include <wtf/text/WTFString.h>

using WTF::Vector;

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {
namespace DOMSupport {

void visibleTextQuads(const VisibleSelection& selection, Vector<FloatQuad>& quads)
{
    if (!selection.isRange())
        return;

    // Make sure that both start and end have valid nodes associated otherwise
    // this can crash. See PR 220628.
    if (!selection.start().anchorNode() || !selection.end().anchorNode())
        return;

    visibleTextQuads(*(selection.firstRange()), quads, true /* useSelectionHeight */);
}

void visibleTextQuads(const Range& range, Vector<FloatQuad>& quads, bool useSelectionHeight)
{
    // Range::textQuads includes hidden text, which we don't want.
    // To work around this, this is a copy of it which skips hidden elements.
    Node* startContainer = range.startContainer();
    Node* endContainer = range.endContainer();

    if (!startContainer || !endContainer)
        return;

    Node* stopNode = range.pastLastNode();
    for (Node* node = range.firstNode(); node && node != stopNode; node = node->traverseNextNode()) {
        RenderObject* r = node->renderer();
        if (!r || !r->isText())
            continue;

        if (r->style()->visibility() != VISIBLE)
            continue;

        RenderText* renderText = toRenderText(r);
        int startOffset = node == startContainer ? range.startOffset() : 0;
        int endOffset = node == endContainer ? range.endOffset() : std::numeric_limits<int>::max();
        renderText->absoluteQuadsForRange(quads, startOffset, endOffset, useSelectionHeight);
    }
}

bool isTextInputElement(Element* element)
{
    return element->isTextFormControl()
           || element->hasTagName(HTMLNames::textareaTag)
           || element->isContentEditable();
}

bool isPasswordElement(const Element* element)
{
    return element && element->hasTagName(HTMLNames::inputTag)
           && static_cast<const HTMLInputElement*>(element)->isPasswordField();
}

WTF::String inputElementText(Element* element)
{
    if (!element)
        return WTF::String();

    WTF::String elementText;
    if (element->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);
        elementText = inputElement->value();
    } else if (element->hasTagName(HTMLNames::textareaTag)) {
        const HTMLTextAreaElement* inputElement = static_cast<const HTMLTextAreaElement*>(element);
        elementText = inputElement->value();
    } else if (element->isContentEditable()) {
        RefPtr<Range> rangeForNode = rangeOfContents(element);
        elementText = rangeForNode.get()->text();
    }
    return elementText;
}

WTF::String webWorksContext(const WebCore::Element* element)
{
    if (!element)
        return WTF::String();

    DEFINE_STATIC_LOCAL(QualifiedName, webworksContextAttr, (nullAtom, "data-webworks-context", nullAtom));
    if (element->fastHasAttribute(webworksContextAttr))
        return element->fastGetAttribute(webworksContextAttr);

    return WTF::String();
}

bool isElementTypePlugin(const Element* element)
{
    if (!element)
        return false;

    if (element->hasTagName(HTMLNames::objectTag)
        || element->hasTagName(HTMLNames::embedTag)
        || element->hasTagName(HTMLNames::appletTag))
        return true;

    return false;
}

HTMLTextFormControlElement* toTextControlElement(Node* node)
{
    if (!(node && node->isElementNode()))
        return 0;

    Element* element = static_cast<Element*>(node);
    if (!element->isFormControlElement())
        return 0;

    HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(element);
    if (!formElement->isTextFormControl())
        return 0;

    return static_cast<HTMLTextFormControlElement*>(formElement);
}

bool isPopupInputField(const Element* element)
{
    return isDateTimeInputField(element) || isColorInputField(element);
}

bool isDateTimeInputField(const Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);

    // The following types have popup's.
    if (inputElement->isDateField()
        || inputElement->isDateTimeField()
        || inputElement->isDateTimeLocalField()
        || inputElement->isTimeField()
        || inputElement->isMonthField())
            return true;

    return false;
}

bool isColorInputField(const Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);

#if ENABLE(INPUT_TYPE_COLOR)
    if (inputElement->isColorControl())
        return true;
#endif

    return false;
}

// This is a Tristate return to allow us to override name matching when
// the attribute is expressly requested for a field. Default indicates
// that the setting is On which is the default but not expressly requested
// for the element being checked. On indicates that it is directly added
// to the element.
AttributeState elementSupportsAutocorrect(const Element* element)
{
    DEFINE_STATIC_LOCAL(QualifiedName, autocorrectAttr, (nullAtom, "autocorrect", nullAtom));
    return elementAttributeState(element, autocorrectAttr);
}

AttributeState elementSupportsAutocomplete(const Element* element)
{
    return elementAttributeState(element, HTMLNames::autocompleteAttr);
}

AttributeState elementSupportsSpellCheck(const Element* element)
{
    return elementAttributeState(element, HTMLNames::spellcheckAttr);
}

AttributeState elementAttributeState(const Element* element, const QualifiedName& attributeName)
{
    // First we check the input item itself. If the attribute is not defined,
    // we check its parent form.
    if (element->fastHasAttribute(attributeName)) {
        AtomicString attributeString = element->fastGetAttribute(attributeName);
        if (equalIgnoringCase(attributeString, "off"))
            return Off;
        if (equalIgnoringCase(attributeString, "on"))
            return On;
        // If we haven't returned, it wasn't set properly. Check the form for an explicit setting
        // because the attribute was provided, but invalid.
    }
    if (element->isFormControlElement()) {
        const HTMLFormControlElement* formElement = static_cast<const HTMLFormControlElement*>(element);
        if (formElement->form() && formElement->form()->fastHasAttribute(attributeName)) {
            AtomicString attributeString = formElement->form()->fastGetAttribute(attributeName);
            if (equalIgnoringCase(attributeString, "off"))
                return Off;
            if (equalIgnoringCase(attributeString, "on"))
                return On;
        }
    }

    return Default;
}

// Check if this is an input field that will be focused & require input support.
bool isTextBasedContentEditableElement(Element* element)
{
    if (!element)
        return false;

    if (element->isReadOnlyNode() || !element->isEnabledFormControl())
        return false;

    if (isPopupInputField(element))
        return false;

    return element->isTextFormControl() || element->isContentEditable();
}

IntRect transformedBoundingBoxForRange(const Range& range)
{
    // Based on Range::boundingBox, which does not handle transforms, and
    // RenderObject::absoluteBoundingBoxRect, which does.
    IntRect result;
    Vector<FloatQuad> quads;
    visibleTextQuads(range, quads);
    const size_t n = quads.size();
    for (size_t i = 0; i < n; ++i)
        result.unite(quads[i].enclosingBoundingBox());

    return result;
}

VisibleSelection visibleSelectionForInputElement(Element* element)
{
    return visibleSelectionForRangeInputElement(element, 0, inputElementText(element).length());
}

VisibleSelection visibleSelectionForRangeInputElement(Element* element, int start, int end)
{
    if (DOMSupport::toTextControlElement(element)) {
        RenderTextControl* textRender = toRenderTextControl(element->renderer());
        if (!textRender)
            return VisibleSelection();

        VisiblePosition startPosition = textRender->visiblePositionForIndex(start);
        VisiblePosition endPosition;
        if (start == end)
            endPosition = startPosition;
        else
            endPosition = textRender->visiblePositionForIndex(end);

        return VisibleSelection(startPosition, endPosition);
    }

    // Must be content editable, generate the range.
    RefPtr<Range> selectionRange = TextIterator::rangeFromLocationAndLength(element, start, end - start);

    if (!selectionRange)
        return VisibleSelection();

    if (start == end)
        return VisibleSelection(selectionRange.get()->startPosition(), DOWNSTREAM);

    VisiblePosition visibleStart(selectionRange->startPosition(), DOWNSTREAM);
    VisiblePosition visibleEnd(selectionRange->endPosition(), SEL_DEFAULT_AFFINITY);

    return VisibleSelection(visibleStart, visibleEnd);
}

static bool matchesReservedStringEmail(const AtomicString& string)
{
    return string.contains("email", false /* caseSensitive */);
}

static bool matchesReservedStringUrl(const AtomicString& string)
{
    return equalIgnoringCase("url", string);
}

bool elementIdOrNameIndicatesEmail(const HTMLInputElement* inputElement)
{
    if (!inputElement)
        return false;

    if (matchesReservedStringEmail(inputElement->getIdAttribute()))
        return true;

    if (inputElement->fastHasAttribute(HTMLNames::nameAttr)) {
        if (matchesReservedStringEmail(inputElement->fastGetAttribute(HTMLNames::nameAttr)))
            return true;
    }

    return false;
}

bool elementIdOrNameIndicatesUrl(const HTMLInputElement* inputElement)
{
    if (!inputElement)
        return false;

    if (matchesReservedStringUrl(inputElement->getIdAttribute()))
        return true;

    if (inputElement->fastHasAttribute(HTMLNames::nameAttr)) {
        if (matchesReservedStringUrl(inputElement->fastGetAttribute(HTMLNames::nameAttr)))
            return true;
    }

    return false;
}

static bool matchesReservedStringPreventingAutocomplete(const AtomicString& string)
{
    if (matchesReservedStringEmail(string)
        || matchesReservedStringUrl(string)
        || string.contains("user", false /* caseSensitive */)
        || string.contains("name", false /* caseSensitive */)
        || string.contains("login", false /* caseSensitive */))
        return true;

    return false;
}

// This checks to see if an input element has a name or id attribute set to
// username or email. These are rough checks to avoid major sites that use
// login fields as input type=text and auto correction interfers with.
bool elementIdOrNameIndicatesNoAutocomplete(const Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    AtomicString idAttribute = element->getIdAttribute();
    if (matchesReservedStringPreventingAutocomplete(idAttribute))
        return true;

    if (element->fastHasAttribute(HTMLNames::nameAttr)) {
        AtomicString nameAttribute = element->fastGetAttribute(HTMLNames::nameAttr);
        if (matchesReservedStringPreventingAutocomplete(nameAttribute))
            return true;
    }

    return false;
}

bool elementPatternIndicatesNumber(const HTMLInputElement* inputElement)
{
    return elementPatternMatches("[0-9]", inputElement);
}

bool elementPatternIndicatesHexadecimal(const HTMLInputElement* inputElement)
{
    return elementPatternMatches("[0-9a-fA-F]", inputElement);
}

bool elementPatternMatches(const char* pattern, const HTMLInputElement* inputElement)
{
    WTF::String patternString(pattern);
    if (!inputElement || patternString.isEmpty())
        return false;

    if (inputElement->fastHasAttribute(HTMLNames::patternAttr)) {
        WTF::String patternAttribute = inputElement->fastGetAttribute(HTMLNames::patternAttr);
        if (patternAttribute.startsWith(patternString)) {
            // The pattern is for hexadecimal, make sure nothing else is permitted.

            // Check if it was an exact match.
            if (patternAttribute.length() == patternString.length())
                return true;

            // Check for *
            if (patternAttribute.length() == patternString.length() + 1 && patternAttribute[patternString.length()] == '*')
                return true;

            // Is the regex specifying a character count?
            if (patternAttribute[patternString.length()] != '{' || !patternAttribute.endsWith("}"))
                return false;

            // Make sure the number in the regex is actually a number.
            unsigned count = 0;
            patternString = patternString + "{%d}";
            return (sscanf(patternAttribute.latin1().data(), patternString.latin1().data() + '\0', &count) == 1) && count > 0;
        }
    }
    return false;
}

IntPoint convertPointToFrame(const Frame* sourceFrame, const Frame* targetFrame, const IntPoint& point, const bool clampToTargetFrame)
{
    ASSERT(sourceFrame && targetFrame);
    if (sourceFrame == targetFrame)
        return point;

    ASSERT(sourceFrame->view() && targetFrame->view());
    ASSERT(targetFrame->tree());

    Frame* targetFrameParent = targetFrame->tree()->parent();
    IntRect targetFrameRect = targetFrame->view()->frameRect();
    IntPoint targetPoint = point;

    // Convert the target frame rect to source window content coordinates. This is only required
    // if the parent frame is not the source. If the parent is the source, subframeRect
    // is already in source content coordinates.
    if (targetFrameParent != sourceFrame)
        targetFrameRect = sourceFrame->view()->windowToContents(targetFrameParent->view()->contentsToWindow(targetFrameRect));

    // Requested point is outside of target frame, return InvalidPoint.
    if (clampToTargetFrame && !targetFrameRect.contains(targetPoint))
        targetPoint = IntPoint(targetPoint.x() < targetFrameRect.x() ? targetFrameRect.x() : std::min(targetPoint.x(), targetFrameRect.maxX()),
                targetPoint.y() < targetFrameRect.y() ? targetFrameRect.y() : std::min(targetPoint.y(), targetFrameRect.maxY()));
    else if (!targetFrameRect.contains(targetPoint))
        return InvalidPoint;

    // Adjust the points to be relative to the target.
    return targetFrame->view()->windowToContents(sourceFrame->view()->contentsToWindow(targetPoint));
}

VisibleSelection visibleSelectionForClosestActualWordStart(const VisibleSelection& selection)
{
    // VisibleSelection validation has a special case when the caret is at the end of a paragraph where
    // it selects the paragraph marker. As well, if the position is at the end of a word, it will select
    // only the space between words. We want to select an actual word so we move the selection to
    // the start of the leftmost word if the character after the selection point is whitespace.

    if (selection.selectionType() != VisibleSelection::RangeSelection) {
        int leftDistance = 0;
        int rightDistance = 0;

        VisibleSelection leftSelection(previousWordPosition(selection.start()));
        bool leftSelectionIsOnWord = !isWhitespace(leftSelection.visibleStart().characterAfter()) && leftSelection.start().containerNode() == selection.start().containerNode();
        if (leftSelectionIsOnWord) {
            VisibleSelection rangeSelection(endOfWord(leftSelection.start()), selection.visibleStart());
            leftDistance = TextIterator::rangeLength(rangeSelection.toNormalizedRange().get());
        }

        VisibleSelection rightSelection = previousWordPosition(nextWordPosition(selection.start()));
        bool rightSelectionIsOnWord = !isWhitespace(rightSelection.visibleStart().characterAfter()) && rightSelection.start().containerNode() == selection.start().containerNode();
        if (rightSelectionIsOnWord) {
            VisibleSelection rangeSelection = VisibleSelection(rightSelection.visibleStart(), selection.visibleStart());
            rightDistance = TextIterator::rangeLength(rangeSelection.toNormalizedRange().get());
        }

        // Make sure we found an actual word. If not, return the original selection.
        if (!leftSelectionIsOnWord && !rightSelectionIsOnWord)
            return selection;

        if (!rightSelectionIsOnWord || (leftSelectionIsOnWord && leftDistance <= rightDistance)) {
            // Left is closer or right is invalid.
            return leftSelection;
        }

        // Right is closer or equal, or left was invalid.
        return rightSelection;
    }

    // No adjustment required.
    return selection;
}

// This function is copied from WebCore/page/Page.cpp.
Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

PassRefPtr<Range> trimWhitespaceFromRange(VisiblePosition startPosition, VisiblePosition endPosition)
{
    if (isEmptyRangeOrAllSpaces(startPosition, endPosition))
        return VisibleSelection(endPosition, endPosition).toNormalizedRange();

    while (isWhitespace(startPosition.characterAfter()))
        startPosition = startPosition.next();

    while (isWhitespace(endPosition.characterBefore()))
        endPosition = endPosition.previous();

    return VisibleSelection(startPosition, endPosition).toNormalizedRange();
}

bool isEmptyRangeOrAllSpaces(VisiblePosition startPosition, VisiblePosition endPosition)
{
    if (startPosition == endPosition)
        return true;

    while (isWhitespace(startPosition.characterAfter())) {
        startPosition = startPosition.next();

        if (startPosition == endPosition)
            return true;
    }

    return false;
}

bool isFixedPositionOrHasFixedPositionAncestor(RenderObject* renderer)
{
    RenderObject* currentRenderer = renderer;
    while (currentRenderer) {

        if (currentRenderer->isOutOfFlowPositioned() && currentRenderer->style()->position() == FixedPosition)
            return true;

        currentRenderer = currentRenderer->parent();
    }

    return false;
}

} // DOMSupport
} // WebKit
} // BlackBerry
