/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012 Igalia S.L.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
 */

#include "config.h"
#include "WebKitAccessibleInterfaceText.h"

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityObject.h"
#include "Document.h"
#include "Editing.h"
#include "FontCascade.h"
#include "FrameView.h"
#include "HTMLParserIdioms.h"
#include "HostWindow.h"
#include "InlineTextBox.h"
#include "NotImplemented.h"
#include "Range.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderText.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleUtil.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

// Text attribute to expose the ARIA 'aria-invalid' attribute. Initially initialized
// to ATK_TEXT_ATTR_INVALID (which means 'invalid' text attribute'), will later on
// hold a reference to the custom registered AtkTextAttribute that we will use.
static AtkTextAttribute atkTextAttributeInvalid = ATK_TEXT_ATTR_INVALID;

static AccessibilityObject* core(AtkText* text)
{
    if (!WEBKIT_IS_ACCESSIBLE(text))
        return 0;

    return &webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(text));
}

static int baselinePositionForRenderObject(RenderObject* renderObject)
{
    // FIXME: This implementation of baselinePosition originates from RenderObject.cpp and was
    // removed in r70072. The implementation looks incorrect though, because this is not the
    // baseline of the underlying RenderObject, but of the AccessibilityRenderObject.
    const FontMetrics& fontMetrics = renderObject->firstLineStyle().fontMetrics();
    return fontMetrics.ascent() + (renderObject->firstLineStyle().computedLineHeight() - fontMetrics.height()) / 2;
}

static AtkAttributeSet* getAttributeSetForAccessibilityObject(const AccessibilityObject* object)
{
    if (!object->isAccessibilityRenderObject())
        return 0;

    RenderObject* renderer = object->renderer();
    auto* style = &renderer->style();

    AtkAttributeSet* result = nullptr;
    GUniquePtr<gchar> buffer(g_strdup_printf("%i", style->computedFontPixelSize()));
    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_SIZE), buffer.get());

    Color bgColor = style->visitedDependentColor(CSSPropertyBackgroundColor);
    if (bgColor.isValid()) {
        auto [r, g, b, a] = bgColor.toSRGBALossy<uint8_t>();
        buffer.reset(g_strdup_printf("%i,%i,%i", r, g, b));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_BG_COLOR), buffer.get());
    }

    Color fgColor = style->visitedDependentColor(CSSPropertyColor);
    if (fgColor.isValid()) {
        auto [r, g, b, a] = fgColor.toSRGBALossy<uint8_t>();
        buffer.reset(g_strdup_printf("%i,%i,%i", r, g, b));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_FG_COLOR), buffer.get());
    }

    int baselinePosition;
    bool includeRise = true;
    switch (style->verticalAlign()) {
    case VerticalAlign::Sub:
        baselinePosition = -1 * baselinePositionForRenderObject(renderer);
        break;
    case VerticalAlign::Super:
        baselinePosition = baselinePositionForRenderObject(renderer);
        break;
    case VerticalAlign::Baseline:
        baselinePosition = 0;
        break;
    default:
        includeRise = false;
        break;
    }

    if (includeRise) {
        buffer.reset(g_strdup_printf("%i", baselinePosition));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_RISE), buffer.get());
    }

    if (!style->textIndent().isUndefined()) {
        int indentation = valueForLength(style->textIndent(), object->size().width());
        buffer.reset(g_strdup_printf("%i", indentation));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_INDENT), buffer.get());
    }

    String fontFamilyName = style->fontCascade().firstFamily();
    if (fontFamilyName.left(8) == "-webkit-")
        fontFamilyName = fontFamilyName.substring(8);

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_FAMILY_NAME), fontFamilyName.utf8().data());

    int fontWeight = static_cast<float>(style->fontCascade().weight());
    if (fontWeight > 0) {
        buffer.reset(g_strdup_printf("%i", fontWeight));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_WEIGHT), buffer.get());
    }

    switch (style->textAlign()) {
    case TextAlignMode::Start:
    case TextAlignMode::End:
        break;
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "left");
        break;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "right");
        break;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "center");
        break;
    case TextAlignMode::Justify:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "fill");
    }

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_UNDERLINE), (style->textDecoration() & TextDecoration::Underline) ? "single" : "none");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_STYLE), style->fontCascade().italic() ? "italic" : "normal");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_STRIKETHROUGH), (style->textDecoration() & TextDecoration::LineThrough) ? "true" : "false");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_INVISIBLE), (style->visibility() == Visibility::Hidden) ? "true" : "false");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_EDITABLE), object->canSetValueAttribute() ? "true" : "false");

    String language = object->language();
    if (!language.isEmpty())
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_LANGUAGE), language.utf8().data());

    String invalidStatus = object->invalidStatus();
    if (invalidStatus != "false") {
        // Register the custom attribute for 'aria-invalid' if not done yet.
        if (atkTextAttributeInvalid == ATK_TEXT_ATTR_INVALID)
            atkTextAttributeInvalid = atk_text_attribute_register("invalid");

        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(atkTextAttributeInvalid), invalidStatus.utf8().data());
    }

    return result;
}

static gint compareAttribute(const AtkAttribute* a, const AtkAttribute* b)
{
    return g_strcmp0(a->name, b->name) || g_strcmp0(a->value, b->value);
}

// Returns an AtkAttributeSet with the elements of attributeSet1 which
// are either not present or different in attributeSet2. Neither
// attributeSet1 nor attributeSet2 should be used after calling this.
static AtkAttributeSet* attributeSetDifference(AtkAttributeSet* attributeSet1, AtkAttributeSet* attributeSet2)
{
    if (!attributeSet2)
        return attributeSet1;

    AtkAttributeSet* currentSet = attributeSet1;
    AtkAttributeSet* found;
    AtkAttributeSet* toDelete = nullptr;

    while (currentSet) {
        found = g_slist_find_custom(attributeSet2, currentSet->data, (GCompareFunc)compareAttribute);
        if (found) {
            AtkAttributeSet* nextSet = currentSet->next;
            toDelete = g_slist_prepend(toDelete, currentSet->data);
            attributeSet1 = g_slist_delete_link(attributeSet1, currentSet);
            currentSet = nextSet;
        } else
            currentSet = currentSet->next;
    }

    atk_attribute_set_free(attributeSet2);
    atk_attribute_set_free(toDelete);
    return attributeSet1;
}

static gchar* webkitAccessibleTextGetText(AtkText*, gint startOffset, gint endOffset);

static guint accessibilityObjectLength(const AccessibilityObject* object)
{
    // Non render objects are not taken into account
    if (!object->isAccessibilityRenderObject())
        return 0;

    // For those objects implementing the AtkText interface we use the
    // well known API to always get the text in a consistent way
    auto* atkObj = ATK_OBJECT(object->wrapper());
    if (ATK_IS_TEXT(atkObj)) {
        GUniquePtr<gchar> text(webkitAccessibleTextGetText(ATK_TEXT(atkObj), 0, -1));
        return g_utf8_strlen(text.get(), -1);
    }

    // Even if we don't expose list markers to Assistive
    // Technologies, we need to have a way to measure their length
    // for those cases when it's needed to take it into account
    // separately (as in getAccessibilityObjectForOffset)
    RenderObject* renderer = object->renderer();
    if (is<RenderListMarker>(renderer)) {
        auto& marker = downcast<RenderListMarker>(*renderer);
        return marker.text().length() + marker.suffix().length();
    }

    return 0;
}

static const AccessibilityObject* getAccessibilityObjectForOffset(const AccessibilityObject* object, guint offset, gint* startOffset, gint* endOffset)
{
    const AccessibilityObject* result;
    guint length = accessibilityObjectLength(object);
    if (length > offset) {
        *startOffset = 0;
        *endOffset = length;
        result = object;
    } else {
        *startOffset = -1;
        *endOffset = -1;
        result = 0;
    }

    if (!object->firstChild())
        return result;

    AccessibilityObject* child = object->firstChild();
    guint currentOffset = 0;
    guint childPosition = 0;
    while (child && currentOffset <= offset) {
        guint childLength = accessibilityObjectLength(child);
        currentOffset = childLength + childPosition;
        if (currentOffset > offset) {
            gint childStartOffset;
            gint childEndOffset;
            const AccessibilityObject* grandChild = getAccessibilityObjectForOffset(child, offset-childPosition,  &childStartOffset, &childEndOffset);
            if (childStartOffset >= 0) {
                *startOffset = childStartOffset + childPosition;
                *endOffset = childEndOffset + childPosition;
                result = grandChild;
            }
        } else {
            childPosition += childLength;
            child = child->nextSibling();
        }
    }
    return result;
}

static AtkAttributeSet* getRunAttributesFromAccessibilityObject(const AccessibilityObject* element, gint offset, gint* startOffset, gint* endOffset)
{
    const AccessibilityObject* child = getAccessibilityObjectForOffset(element, offset, startOffset, endOffset);
    if (!child) {
        *startOffset = -1;
        *endOffset = -1;
        return 0;
    }

    AtkAttributeSet* defaultAttributes = getAttributeSetForAccessibilityObject(element);
    AtkAttributeSet* childAttributes = getAttributeSetForAccessibilityObject(child);

    return attributeSetDifference(childAttributes, defaultAttributes);
}

static IntRect textExtents(AtkText* text, gint startOffset, gint length, AtkCoordType coords)
{
    GUniquePtr<char> textContent(webkitAccessibleTextGetText(text, startOffset, -1));
    gint textLength = g_utf8_strlen(textContent.get(), -1);

    // The first case (endOffset of -1) should work, but seems broken for all Gtk+ apps.
    gint rangeLength = length;
    if (rangeLength < 0 || rangeLength > textLength)
        rangeLength = textLength;
    AccessibilityObject* coreObject = core(text);

    IntRect extents = coreObject->doAXBoundsForRange(PlainTextRange(startOffset, rangeLength));
    switch (coords) {
    case ATK_XY_SCREEN:
        if (Document* document = coreObject->document())
            extents = document->view()->contentsToScreen(extents);
        break;
    case ATK_XY_WINDOW:
        // No-op
        break;
#if ATK_CHECK_VERSION(2, 30, 0)
    case ATK_XY_PARENT:
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    return extents;
}

static int offsetAdjustmentForListItem(const AXCoreObject* object)
{
    // We need to adjust the offsets for the list item marker in
    // Left-To-Right text, since we expose it together with the text.
    RenderObject* renderer = object->renderer();
    if (is<RenderListItem>(renderer) && renderer->style().direction() == TextDirection::LTR)
        return downcast<RenderListItem>(*renderer).markerTextWithSuffix().length();

    return 0;
}

static int webCoreOffsetToAtkOffset(const AXCoreObject* object, int offset)
{
    if (!object->isListItem())
        return offset;

    return offset + offsetAdjustmentForListItem(object);
}

static int atkOffsetToWebCoreOffset(AtkText* text, int offset)
{
    AccessibilityObject* coreObject = core(text);
    if (!coreObject || !coreObject->isListItem())
        return offset;

    return offset - offsetAdjustmentForListItem(coreObject);
}

static Node* getNodeForAccessibilityObject(AccessibilityObject* coreObject)
{
    if (!coreObject->isNativeTextControl())
        return coreObject->node();

    // For text controls, we get the first visible position on it (which will
    // belong to its inner element, unreachable from the DOM) and return its
    // parent node, so we have a "bounding node" for the accessibility object.
    VisiblePosition positionInTextControlInnerElement = coreObject->visiblePositionForIndex(0, true);
    Node* innerMostNode = positionInTextControlInnerElement.deepEquivalent().anchorNode();
    if (!innerMostNode)
        return 0;

    return innerMostNode->parentNode();
}

static void getSelectionOffsetsForObject(AccessibilityObject* coreObject, VisibleSelection& selection, gint& startOffset, gint& endOffset)
{
    // Default values, unless the contrary is proved.
    startOffset = 0;
    endOffset = 0;

    Node* node = getNodeForAccessibilityObject(coreObject);
    if (!node)
        return;

    if (selection.isNone())
        return;

    // We need to limit our search to positions that fall inside the domain of the current object.
    Position firstValidPosition = firstPositionInOrBeforeNode(node->firstDescendant());
    Position lastValidPosition = lastPositionInOrAfterNode(node->lastDescendant());

    // Find the proper range for the selection that falls inside the object.
    Position nodeRangeStart = selection.start();
    if (comparePositions(nodeRangeStart, firstValidPosition) < 0)
        nodeRangeStart = firstValidPosition;

    Position nodeRangeEnd = selection.end();
    if (comparePositions(nodeRangeEnd, lastValidPosition) > 0)
        nodeRangeEnd = lastValidPosition;

    // Calculate position of the selected range inside the object.
    Position parentFirstPosition = firstPositionInOrBeforeNode(node);
    auto rangeInParent = Range::create(node->document(), parentFirstPosition, nodeRangeStart);

    // Set values for start offsets and calculate initial range length.
    // These values might be adjusted later to cover special cases.
    startOffset = webCoreOffsetToAtkOffset(coreObject, characterCount(rangeInParent.get(), TextIteratorEmitsCharactersBetweenAllVisiblePositions));
    auto nodeRange = Range::create(node->document(), nodeRangeStart, nodeRangeEnd);
    int rangeLength = characterCount(nodeRange.get(), TextIteratorEmitsCharactersBetweenAllVisiblePositions);

    // Special cases that are only relevant when working with *_END boundaries.
    if (selection.affinity() == UPSTREAM) {
        VisiblePosition visibleStart(nodeRangeStart, UPSTREAM);
        VisiblePosition visibleEnd(nodeRangeEnd, UPSTREAM);

        // We need to adjust offsets when finding wrapped lines so the position at the end
        // of the line is properly taking into account when calculating the offsets.
        if (isEndOfLine(visibleStart) && !lineBreakExistsAtVisiblePosition(visibleStart)) {
            if (isStartOfLine(visibleStart.next()))
                rangeLength++;

            if (!isEndOfBlock(visibleStart))
                startOffset = std::max(startOffset - 1, 0);
        }

        if (isEndOfLine(visibleEnd) && !lineBreakExistsAtVisiblePosition(visibleEnd) && !isEndOfBlock(visibleEnd))
            rangeLength--;
    }

    endOffset = std::min(startOffset + rangeLength, static_cast<int>(accessibilityObjectLength(coreObject)));
}

static gchar* webkitAccessibleTextGetText(AtkText* text, gint startOffset, gint endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    AccessibilityObject* coreObject = core(text);

#if ENABLE(INPUT_TYPE_COLOR)
    if (coreObject->roleValue() == AccessibilityRole::ColorWell) {
        auto color = convertToComponentFloats(coreObject->colorValue());
        return g_strdup_printf("rgb %7.5f %7.5f %7.5f 1", color.red, color.green, color.blue);
    }
#endif

    String ret;
    if (coreObject->isTextControl())
        ret = coreObject->doAXStringForRange(PlainTextRange(0, endOffset));
    else {
        ret = coreObject->stringValue();
        if (!ret)
            ret = coreObject->textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren));
    }

    // Prefix a item number/bullet if needed
    int actualEndOffset = endOffset == -1 ? ret.length() : endOffset;
    if (coreObject->roleValue() == AccessibilityRole::ListItem) {
        RenderObject* objRenderer = coreObject->renderer();
        if (is<RenderListItem>(objRenderer)) {
            String markerText = downcast<RenderListItem>(*objRenderer).markerTextWithSuffix();
            ret = objRenderer->style().direction() == TextDirection::LTR ? markerText + ret : ret + markerText;
            if (endOffset == -1)
                actualEndOffset = ret.length() + markerText.length();
        }
    }

    ret = ret.substring(startOffset, actualEndOffset - startOffset);
    return g_strdup(ret.utf8().data());
}

enum GetTextRelativePosition {
    GetTextPositionAt,
    GetTextPositionBefore,
    GetTextPositionAfter
};

// Convenience function to be used in early returns.
static char* emptyTextSelectionAtOffset(int offset, int* startOffset, int* endOffset)
{
    *startOffset = offset;
    *endOffset = offset;
    return g_strdup("");
}

static char* webkitAccessibleTextGetChar(AtkText* text, int offset, GetTextRelativePosition textPosition, int* startOffset, int* endOffset)
{
    int actualOffset = offset;
    if (textPosition == GetTextPositionBefore)
        actualOffset--;
    else if (textPosition == GetTextPositionAfter)
        actualOffset++;

    GUniquePtr<char> textData(webkitAccessibleTextGetText(text, 0, -1));
    int textLength = g_utf8_strlen(textData.get(), -1);

    *startOffset = std::max(0, actualOffset);
    *startOffset = std::min(*startOffset, textLength);

    *endOffset = std::max(0, actualOffset + 1);
    *endOffset = std::min(*endOffset, textLength);

    if (*startOffset == *endOffset)
        return g_strdup("");

    return g_utf8_substring(textData.get(), *startOffset, *endOffset);
}

static VisiblePosition nextWordStartPosition(const VisiblePosition &position)
{
    VisiblePosition positionAfterCurrentWord = nextWordPosition(position);

    // In order to skip spaces when moving right, we advance one word further
    // and then move one word back. This will put us at the beginning of the
    // word following.
    VisiblePosition positionAfterSpacingAndFollowingWord = nextWordPosition(positionAfterCurrentWord);

    if (positionAfterSpacingAndFollowingWord != positionAfterCurrentWord)
        positionAfterCurrentWord = previousWordPosition(positionAfterSpacingAndFollowingWord);

    bool movingBackwardsMovedPositionToStartOfCurrentWord = positionAfterCurrentWord == previousWordPosition(nextWordPosition(position));
    if (movingBackwardsMovedPositionToStartOfCurrentWord)
        positionAfterCurrentWord = positionAfterSpacingAndFollowingWord;

    return positionAfterCurrentWord;
}

static VisiblePosition previousWordEndPosition(const VisiblePosition &position)
{
    // We move forward and then backward to position ourselves at the beginning
    // of the current word for this boundary, making the most of the semantics
    // of previousWordPosition() and nextWordPosition().
    VisiblePosition positionAtStartOfCurrentWord = previousWordPosition(nextWordPosition(position));
    VisiblePosition positionAtPreviousWord = previousWordPosition(position);

    // Need to consider special cases (punctuation) when we are in the last word of a sentence.
    if (isStartOfWord(position) && positionAtPreviousWord != position && positionAtPreviousWord == positionAtStartOfCurrentWord)
        return nextWordPosition(positionAtStartOfCurrentWord);

    // In order to skip spaces when moving left, we advance one word backwards
    // and then move one word forward. This will put us at the beginning of
    // the word following.
    VisiblePosition positionBeforeSpacingAndPreceedingWord = previousWordPosition(positionAtStartOfCurrentWord);

    if (positionBeforeSpacingAndPreceedingWord != positionAtStartOfCurrentWord)
        positionAtStartOfCurrentWord = nextWordPosition(positionBeforeSpacingAndPreceedingWord);

    bool movingForwardMovedPositionToEndOfCurrentWord = nextWordPosition(positionAtStartOfCurrentWord) == previousWordPosition(nextWordPosition(position));
    if (movingForwardMovedPositionToEndOfCurrentWord)
        positionAtStartOfCurrentWord = positionBeforeSpacingAndPreceedingWord;

    return positionAtStartOfCurrentWord;
}

static VisibleSelection wordAtPositionForAtkBoundary(const AccessibilityObject* /*coreObject*/, const VisiblePosition& position, AtkTextBoundary boundaryType)
{
    VisiblePosition startPosition;
    VisiblePosition endPosition;

    switch (boundaryType) {
    case ATK_TEXT_BOUNDARY_WORD_START:
        // isStartOfWord() returns true both when at the beginning of a "real" word
        // as when at the beginning of a whitespace range between two "real" words,
        // since that whitespace is considered a "word" as well. And in case we are
        // already at the beginning of a "real" word we do not need to look backwards.
        if (isStartOfWord(position) && deprecatedIsEditingWhitespace(position.characterBefore()))
            startPosition = position;
        else
            startPosition = previousWordPosition(position);
        endPosition = nextWordStartPosition(startPosition);

        // We need to make sure that we look for the word in the current line when
        // we are at the beginning of a new line, and not look into the previous one
        // at all, which might happen when lines belong to different nodes.
        if (isStartOfLine(position) && isStartOfLine(endPosition)) {
            startPosition = endPosition;
            endPosition = nextWordStartPosition(startPosition);
        }
        break;

    case ATK_TEXT_BOUNDARY_WORD_END:
        startPosition = previousWordEndPosition(position);
        endPosition = nextWordPosition(startPosition);
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    VisibleSelection selectedWord(startPosition, endPosition);

    // We mark the selection as 'upstream' so we can use that information later,
    // when finding the actual offsets in getSelectionOffsetsForObject().
    if (boundaryType == ATK_TEXT_BOUNDARY_WORD_END)
        selectedWord.setAffinity(UPSTREAM);

    return selectedWord;
}

static int numberOfReplacedElementsBeforeOffset(AtkText* text, unsigned offset)
{
    GUniquePtr<char> textForObject(webkitAccessibleTextGetText(text, 0, offset));
    String textBeforeOffset = String::fromUTF8(textForObject.get());

    int count = 0;
    size_t index = textBeforeOffset.find(objectReplacementCharacter, 0);
    while (index < offset && index != WTF::notFound) {
        index = textBeforeOffset.find(objectReplacementCharacter, index + 1);
        count++;
    }
    return count;
}

static char* webkitAccessibleTextWordForBoundary(AtkText* text, int offset, AtkTextBoundary boundaryType, GetTextRelativePosition textPosition, int* startOffset, int* endOffset)
{
    AccessibilityObject* coreObject = core(text);
    Document* document = coreObject->document();
    if (!document)
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    Node* node = getNodeForAccessibilityObject(coreObject);
    if (!node)
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    int actualOffset = atkOffsetToWebCoreOffset(text, offset);

    // Besides of the usual conversion from ATK offsets to WebCore offsets,
    // we need to consider the potential embedded objects that might have been
    // inserted in the text exposed through AtkText when calculating the offset.
    actualOffset -= numberOfReplacedElementsBeforeOffset(text, actualOffset);

    VisiblePosition caretPosition = coreObject->visiblePositionForIndex(actualOffset);
    VisibleSelection currentWord = wordAtPositionForAtkBoundary(coreObject, caretPosition, boundaryType);

    // Take into account other relative positions, if needed, by
    // calculating the new position that we would need to consider.
    VisiblePosition newPosition = caretPosition;
    switch (textPosition) {
    case GetTextPositionAt:
        break;

    case GetTextPositionBefore:
        // Early return if asking for the previous word while already at the beginning.
        if (isFirstVisiblePositionInNode(currentWord.visibleStart(), node))
            return emptyTextSelectionAtOffset(0, startOffset, endOffset);

        if (isStartOfLine(currentWord.end()))
            newPosition = currentWord.visibleStart().previous();
        else
            newPosition = startOfWord(currentWord.start(), LeftWordIfOnBoundary);
        break;

    case GetTextPositionAfter:
        // Early return if asking for the following word while already at the end.
        if (isLastVisiblePositionInNode(currentWord.visibleEnd(), node))
            return emptyTextSelectionAtOffset(accessibilityObjectLength(coreObject), startOffset, endOffset);

        if (isEndOfLine(currentWord.end()))
            newPosition = currentWord.visibleEnd().next();
        else
            newPosition = endOfWord(currentWord.end(), RightWordIfOnBoundary);
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    // Determine the relevant word we are actually interested in
    // and calculate the ATK offsets for it, then return everything.
    VisibleSelection selectedWord = newPosition != caretPosition ? wordAtPositionForAtkBoundary(coreObject, newPosition, boundaryType) : currentWord;
    getSelectionOffsetsForObject(coreObject, selectedWord, *startOffset, *endOffset);
    return webkitAccessibleTextGetText(text, *startOffset, *endOffset);
}

static bool isSentenceBoundary(const VisiblePosition &pos)
{
    if (pos.isNull())
        return false;

    // It's definitely a sentence boundary if there's nothing before.
    if (pos.previous().isNull())
        return true;

    // We go backwards and forward to make sure about this.
    VisiblePosition startOfPreviousSentence = startOfSentence(pos);
    return startOfPreviousSentence.isNotNull() && pos == endOfSentence(startOfPreviousSentence);
}

static bool isWhiteSpaceBetweenSentences(const VisiblePosition& position)
{
    if (position.isNull())
        return false;

    if (!deprecatedIsEditingWhitespace(position.characterAfter()))
        return false;

    VisiblePosition startOfWhiteSpace = startOfWord(position, RightWordIfOnBoundary);
    VisiblePosition endOfWhiteSpace = endOfWord(startOfWhiteSpace, RightWordIfOnBoundary);
    if (!isSentenceBoundary(startOfWhiteSpace) && !isSentenceBoundary(endOfWhiteSpace))
        return false;

    return comparePositions(startOfWhiteSpace, position) <= 0 && comparePositions(endOfWhiteSpace, position) >= 0;
}

static VisibleSelection sentenceAtPositionForAtkBoundary(const AccessibilityObject*, const VisiblePosition& position, AtkTextBoundary boundaryType)
{
    VisiblePosition startPosition;
    VisiblePosition endPosition;

    bool isAtStartOfSentenceForEndBoundary = isWhiteSpaceBetweenSentences(position) || isSentenceBoundary(position);
    if (boundaryType == ATK_TEXT_BOUNDARY_SENTENCE_START || !isAtStartOfSentenceForEndBoundary) {
        startPosition = isSentenceBoundary(position) ? position : startOfSentence(position);
        // startOfSentence might stop at a linebreak in the HTML source code,
        // but we don't want to stop there yet, so keep going.
        while (!isSentenceBoundary(startPosition) && isHTMLLineBreak(startPosition.characterBefore()))
            startPosition = startOfSentence(startPosition);

        endPosition = endOfSentence(startPosition);
    }

    if (boundaryType == ATK_TEXT_BOUNDARY_SENTENCE_END) {
        if (isAtStartOfSentenceForEndBoundary) {
            startPosition = position;
            endPosition = endOfSentence(endOfWord(position, RightWordIfOnBoundary));
        }

        // startOfSentence returns a position after any white space previous to
        // the sentence, so we might need to adjust that offset for this boundary.
        if (deprecatedIsEditingWhitespace(startPosition.characterBefore()))
            startPosition = startOfWord(startPosition, LeftWordIfOnBoundary);

        // endOfSentence returns a position after any white space after the
        // sentence, so we might need to adjust that offset for this boundary.
        if (deprecatedIsEditingWhitespace(endPosition.characterBefore()))
            endPosition = startOfWord(endPosition, LeftWordIfOnBoundary);

        // Finally, do some additional adjustments that might be needed if
        // positions are at the start or the end of a line.
        if (isStartOfLine(startPosition) && !isStartOfBlock(startPosition))
            startPosition = startPosition.previous();
        if (isStartOfLine(endPosition) && !isStartOfBlock(endPosition))
            endPosition = endPosition.previous();
    }

    VisibleSelection selectedSentence(startPosition, endPosition);

    // We mark the selection as 'upstream' so we can use that information later,
    // when finding the actual offsets in getSelectionOffsetsForObject().
    if (boundaryType == ATK_TEXT_BOUNDARY_SENTENCE_END)
        selectedSentence.setAffinity(UPSTREAM);

    return selectedSentence;
}

static char* webkitAccessibleTextSentenceForBoundary(AtkText* text, int offset, AtkTextBoundary boundaryType, GetTextRelativePosition textPosition, int* startOffset, int* endOffset)
{
    AccessibilityObject* coreObject = core(text);
    Document* document = coreObject->document();
    if (!document)
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    Node* node = getNodeForAccessibilityObject(coreObject);
    if (!node)
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    int actualOffset = atkOffsetToWebCoreOffset(text, offset);

    // Besides of the usual conversion from ATK offsets to WebCore offsets,
    // we need to consider the potential embedded objects that might have been
    // inserted in the text exposed through AtkText when calculating the offset.
    actualOffset -= numberOfReplacedElementsBeforeOffset(text, actualOffset);

    VisiblePosition caretPosition = coreObject->visiblePositionForIndex(actualOffset);
    VisibleSelection currentSentence = sentenceAtPositionForAtkBoundary(coreObject, caretPosition, boundaryType);

    // Take into account other relative positions, if needed, by
    // calculating the new position that we would need to consider.
    VisiblePosition newPosition = caretPosition;
    switch (textPosition) {
    case GetTextPositionAt:
        break;

    case GetTextPositionBefore:
        // Early return if asking for the previous sentence while already at the beginning.
        if (isFirstVisiblePositionInNode(currentSentence.visibleStart(), node))
            return emptyTextSelectionAtOffset(0, startOffset, endOffset);
        newPosition = currentSentence.visibleStart().previous();
        break;

    case GetTextPositionAfter:
        // Early return if asking for the following word while already at the end.
        if (isLastVisiblePositionInNode(currentSentence.visibleEnd(), node))
            return emptyTextSelectionAtOffset(accessibilityObjectLength(coreObject), startOffset, endOffset);
        newPosition = currentSentence.visibleEnd().next();
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    // Determine the relevant sentence we are actually interested in
    // and calculate the ATK offsets for it, then return everything.
    VisibleSelection selectedSentence = newPosition != caretPosition ? sentenceAtPositionForAtkBoundary(coreObject, newPosition, boundaryType) : currentSentence;
    getSelectionOffsetsForObject(coreObject, selectedSentence, *startOffset, *endOffset);
    return webkitAccessibleTextGetText(text, *startOffset, *endOffset);
}

static VisibleSelection lineAtPositionForAtkBoundary(const AccessibilityObject* coreObject, const VisiblePosition& position, AtkTextBoundary boundaryType)
{
    UNUSED_PARAM(coreObject);
    VisiblePosition startPosition;
    VisiblePosition endPosition;

    switch (boundaryType) {
    case ATK_TEXT_BOUNDARY_LINE_START:
        startPosition = isStartOfLine(position) ? position : logicalStartOfLine(position);
        endPosition = logicalEndOfLine(position);

        // In addition to checking that we are not at the end of a block, we need
        // to check that endPosition has not UPSTREAM affinity, since that would
        // cause trouble inside of text controls (we would be advancing too much).
        if (!isEndOfBlock(endPosition) && endPosition.affinity() != UPSTREAM)
            endPosition = endPosition.next();
        break;

    case ATK_TEXT_BOUNDARY_LINE_END:
        startPosition = isEndOfLine(position) ? position : logicalStartOfLine(position);
        if (!isStartOfBlock(startPosition))
            startPosition = startPosition.previous();
        endPosition = logicalEndOfLine(position);
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    VisibleSelection selectedLine(startPosition, endPosition);

    // We mark the selection as 'upstream' so we can use that information later,
    // when finding the actual offsets in getSelectionOffsetsForObject().
    if (boundaryType == ATK_TEXT_BOUNDARY_LINE_END)
        selectedLine.setAffinity(UPSTREAM);

    return selectedLine;
}

static char* webkitAccessibleTextLineForBoundary(AtkText* text, int offset, AtkTextBoundary boundaryType, GetTextRelativePosition textPosition, int* startOffset, int* endOffset)
{
    AccessibilityObject* coreObject = core(text);
    Document* document = coreObject->document();
    if (!document)
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    Node* node = getNodeForAccessibilityObject(coreObject);
    if (!node)
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    int actualOffset = atkOffsetToWebCoreOffset(text, offset);

    // Besides the usual conversion from ATK offsets to WebCore offsets,
    // we need to consider the potential embedded objects that might have been
    // inserted in the text exposed through AtkText when calculating the offset.
    actualOffset -= numberOfReplacedElementsBeforeOffset(text, actualOffset);

    VisiblePosition caretPosition = coreObject->visiblePositionForIndex(actualOffset);
    VisibleSelection currentLine = lineAtPositionForAtkBoundary(coreObject, caretPosition, boundaryType);

    // Take into account other relative positions, if needed, by
    // calculating the new position that we would need to consider.
    VisiblePosition newPosition = caretPosition;
    switch (textPosition) {
    case GetTextPositionAt:
        // No need to do additional work if we are using the "at" position, we just
        // explicitly list this case option to catch invalid values in the default case.
        break;

    case GetTextPositionBefore:
        // Early return if asking for the previous line while already at the beginning.
        if (isFirstVisiblePositionInNode(currentLine.visibleStart(), node))
            return emptyTextSelectionAtOffset(0, startOffset, endOffset);
        newPosition = currentLine.visibleStart().previous();
        break;

    case GetTextPositionAfter:
        // Early return if asking for the following word while already at the end.
        if (isLastVisiblePositionInNode(currentLine.visibleEnd(), node))
            return emptyTextSelectionAtOffset(accessibilityObjectLength(coreObject), startOffset, endOffset);
        newPosition = currentLine.visibleEnd().next();
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    // Determine the relevant line we are actually interested in
    // and calculate the ATK offsets for it, then return everything.
    VisibleSelection selectedLine = newPosition != caretPosition ? lineAtPositionForAtkBoundary(coreObject, newPosition, boundaryType) : currentLine;
    getSelectionOffsetsForObject(coreObject, selectedLine, *startOffset, *endOffset);

    // We might need to adjust the start or end offset to include the list item marker,
    // if present, when printing the first or the last full line for a list item.
    RenderObject* renderer = coreObject->renderer();
    if (renderer->isListItem()) {
        // For Left-to-Right, the list item marker is at the beginning of the exposed text.
        if (renderer->style().direction() == TextDirection::LTR && isFirstVisiblePositionInNode(selectedLine.visibleStart(), node))
            *startOffset = 0;

        // For Right-to-Left, the list item marker is at the end of the exposed text.
        if (renderer->style().direction() == TextDirection::RTL && isLastVisiblePositionInNode(selectedLine.visibleEnd(), node))
            *endOffset = accessibilityObjectLength(coreObject);
    }

    return webkitAccessibleTextGetText(text, *startOffset, *endOffset);
}

static gchar* webkitAccessibleTextGetTextForOffset(AtkText* text, gint offset, AtkTextBoundary boundaryType, GetTextRelativePosition textPosition, gint* startOffset, gint* endOffset)
{
    AccessibilityObject* coreObject = core(text);
    if (!coreObject || !coreObject->isAccessibilityRenderObject())
        return emptyTextSelectionAtOffset(0, startOffset, endOffset);

    switch (boundaryType) {
    case ATK_TEXT_BOUNDARY_CHAR:
        return webkitAccessibleTextGetChar(text, offset, textPosition, startOffset, endOffset);

    case ATK_TEXT_BOUNDARY_WORD_START:
    case ATK_TEXT_BOUNDARY_WORD_END:
        return webkitAccessibleTextWordForBoundary(text, offset, boundaryType, textPosition, startOffset, endOffset);

    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
        return webkitAccessibleTextLineForBoundary(text, offset, boundaryType, textPosition, startOffset, endOffset);

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
        return webkitAccessibleTextSentenceForBoundary(text, offset, boundaryType, textPosition, startOffset, endOffset);

    default:
        ASSERT_NOT_REACHED();
    }

    // This should never be reached.
    return 0;
}

static gchar* webkitAccessibleTextGetTextAfterOffset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    return webkitAccessibleTextGetTextForOffset(text, offset, boundaryType, GetTextPositionAfter, startOffset, endOffset);
}

static gchar* webkitAccessibleTextGetTextAtOffset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    return webkitAccessibleTextGetTextForOffset(text, offset, boundaryType, GetTextPositionAt, startOffset, endOffset);
}

static gchar* webkitAccessibleTextGetTextBeforeOffset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    return webkitAccessibleTextGetTextForOffset(text, offset, boundaryType, GetTextPositionBefore, startOffset, endOffset);
}

static gunichar webkitAccessibleTextGetCharacterAtOffset(AtkText* text, gint)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    notImplemented();
    return 0;
}

static gint webkitAccessibleTextGetCaretOffset(AtkText* text)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    // coreObject is the unignored object whose offset the caller is requesting.
    // focusedObject is the object with the caret. It is likely ignored -- unless it's a link.
    AXCoreObject* coreObject = core(text);
    if (!coreObject->isAccessibilityRenderObject())
        return 0;

    // We need to make sure we pass a valid object as reference.
    if (coreObject->accessibilityIsIgnored())
        coreObject = coreObject->parentObjectUnignored();
    if (!coreObject)
        return 0;

    int offset;
    if (!objectFocusedAndCaretOffsetUnignored(coreObject, offset))
        return 0;

    return webCoreOffsetToAtkOffset(coreObject, offset);
}

static AtkAttributeSet* webkitAccessibleTextGetRunAttributes(AtkText* text, gint offset, gint* startOffset, gint* endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    AccessibilityObject* coreObject = core(text);
    AtkAttributeSet* result;

    if (!coreObject) {
        *startOffset = 0;
        *endOffset = atk_text_get_character_count(text);
        return 0;
    }

    if (offset == -1)
        offset = atk_text_get_caret_offset(text);

    result = getRunAttributesFromAccessibilityObject(coreObject, offset, startOffset, endOffset);

    if (*startOffset < 0) {
        *startOffset = offset;
        *endOffset = offset;
    }

    return result;
}

static AtkAttributeSet* webkitAccessibleTextGetDefaultAttributes(AtkText* text)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    AccessibilityObject* coreObject = core(text);
    if (!coreObject || !coreObject->isAccessibilityRenderObject())
        return 0;

    return getAttributeSetForAccessibilityObject(coreObject);
}

static void webkitAccessibleTextGetCharacterExtents(AtkText* text, gint offset, gint* x, gint* y, gint* width, gint* height, AtkCoordType coords)
{
    g_return_if_fail(ATK_TEXT(text));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text));

    IntRect extents = textExtents(text, offset, 1, coords);
    *x = extents.x();
    *y = extents.y();
    *width = extents.width();
    *height = extents.height();
}

static void webkitAccessibleTextGetRangeExtents(AtkText* text, gint startOffset, gint endOffset, AtkCoordType coords, AtkTextRectangle* rect)
{
    g_return_if_fail(ATK_TEXT(text));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text));

    IntRect extents = textExtents(text, startOffset, endOffset - startOffset, coords);
    rect->x = extents.x();
    rect->y = extents.y();
    rect->width = extents.width();
    rect->height = extents.height();
}

static gint webkitAccessibleTextGetCharacterCount(AtkText* text)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    return accessibilityObjectLength(core(text));
}

static gint webkitAccessibleTextGetOffsetAtPoint(AtkText* text, gint x, gint y, AtkCoordType)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    // FIXME: Use the AtkCoordType
    // TODO: Is it correct to ignore range.length?
    IntPoint pos(x, y);
    PlainTextRange range = core(text)->doAXRangeForPosition(pos);
    return range.start;
}

static gint webkitAccessibleTextGetNSelections(AtkText* text)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();

    // Only range selections are needed for the purpose of this method
    if (!selection.isRange())
        return 0;

    // We don't support multiple selections for now, so there's only
    // two possibilities
    // Also, we don't want to do anything if the selection does not
    // belong to the currently selected object. We have to check since
    // there's no way to get the selection for a given object, only
    // the global one (the API is a bit confusing)
    return selectionBelongsToObject(coreObject, selection) ? 1 : 0;
}

static gchar* webkitAccessibleTextGetSelection(AtkText* text, gint selectionNum, gint* startOffset, gint* endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), 0);

    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    if (selectionNum)
        return 0;

    // Get the offsets of the selection for the selected object
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();
    getSelectionOffsetsForObject(coreObject, selection, *startOffset, *endOffset);

    // Return 0 instead of "", as that's the expected result for
    // this AtkText method when there's no selection
    if (*startOffset == *endOffset)
        return 0;

    return webkitAccessibleTextGetText(text, *startOffset, *endOffset);
}

static gboolean webkitAccessibleTextAddSelection(AtkText* text, gint, gint)
{
    g_return_val_if_fail(ATK_TEXT(text), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), FALSE);

    notImplemented();
    return FALSE;
}

static gboolean webkitAccessibleTextSetSelection(AtkText* text, gint selectionNum, gint startOffset, gint endOffset)
{
    g_return_val_if_fail(ATK_TEXT(text), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), FALSE);

    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    if (selectionNum)
        return FALSE;

    AccessibilityObject* coreObject = core(text);
    if (!coreObject->isAccessibilityRenderObject())
        return FALSE;

    // Consider -1 and out-of-bound values and correct them to length
    gint textCount = webkitAccessibleTextGetCharacterCount(text);
    if (startOffset < 0 || startOffset > textCount)
        startOffset = textCount;
    if (endOffset < 0 || endOffset > textCount)
        endOffset = textCount;

    // We need to adjust the offsets for the list item marker.
    int offsetAdjustment = offsetAdjustmentForListItem(coreObject);
    if (offsetAdjustment) {
        if (startOffset < offsetAdjustment || endOffset < offsetAdjustment)
            return FALSE;

        startOffset = atkOffsetToWebCoreOffset(text, startOffset);
        endOffset = atkOffsetToWebCoreOffset(text, endOffset);
    }

    PlainTextRange textRange(startOffset, endOffset - startOffset);
    VisiblePositionRange range = coreObject->visiblePositionRangeForRange(textRange);
    if (range.isNull())
        return FALSE;

    coreObject->setSelectedVisiblePositionRange(range);
    return TRUE;
}

static gboolean webkitAccessibleTextRemoveSelection(AtkText* text, gint selectionNum)
{
    g_return_val_if_fail(ATK_TEXT(text), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(text), FALSE);

    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    if (selectionNum)
        return FALSE;

    // Do nothing if current selection doesn't belong to the object
    if (!webkitAccessibleTextGetNSelections(text))
        return FALSE;

    // Set a new 0-sized selection to the caret position, in order
    // to simulate selection removal (GAIL style)
    gint caretOffset = webkitAccessibleTextGetCaretOffset(text);
    return webkitAccessibleTextSetSelection(text, selectionNum, caretOffset, caretOffset);
}

static gboolean webkitAccessibleTextSetCaretOffset(AtkText* text, gint offset)
{
    // Internally, setting the caret offset is equivalent to set a zero-length
    // selection, so delegate in that implementation and void duplicated code.
    return webkitAccessibleTextSetSelection(text, 0, offset, offset);
}

static gchar* webkitAccessibleTextGetStringAtOffset(AtkText* text, gint offset, AtkTextGranularity granularity, gint* startOffset, gint* endOffset)
{
    // This new API has been designed to simplify the AtkText interface and it has been
    // designed to keep exactly the same behaviour the atk_text_get_text_at_text() for
    // ATK_TEXT_BOUNDARY_*_START boundaries, so for now we just need to translate the
    // granularity to the right old boundary and reuse the code for the old API.
    // However, this should be simplified later on (and a lot of code removed) once
    // WebKitGTK depends on ATK >= 2.9.4 *and* can safely assume that a version of
    // AT-SPI2 new enough not to include the old APIs is being used. But until then,
    // we will have to live with both the old and new APIs implemented here.
    // FIXME: WebKit nowadays depends on much newer ATK and we can safely assume AT-SPI2
    // isn't ancient. But whoever wrote this code didn't use ATK_CHECK_VERSION() guards,
    // so it's unclear what is supposed to be changed here.
    AtkTextBoundary boundaryType = ATK_TEXT_BOUNDARY_CHAR;
    switch (granularity) {
    case ATK_TEXT_GRANULARITY_CHAR:
        break;

    case ATK_TEXT_GRANULARITY_WORD:
        boundaryType = ATK_TEXT_BOUNDARY_WORD_START;
        break;

    case ATK_TEXT_GRANULARITY_SENTENCE:
        boundaryType = ATK_TEXT_BOUNDARY_SENTENCE_START;
        break;

    case ATK_TEXT_GRANULARITY_LINE:
        boundaryType = ATK_TEXT_BOUNDARY_LINE_START;
        break;

    case ATK_TEXT_GRANULARITY_PARAGRAPH:
        // FIXME: This has not been a need with the old AtkText API, which means ATs won't
        // need it yet for some time, so we can skip it for now.
        notImplemented();
        return g_strdup("");

    default:
        ASSERT_NOT_REACHED();
    }

    return webkitAccessibleTextGetTextForOffset(text, offset, boundaryType, GetTextPositionAt, startOffset, endOffset);
}

void webkitAccessibleTextInterfaceInit(AtkTextIface* iface)
{
    iface->get_text = webkitAccessibleTextGetText;
    iface->get_text_after_offset = webkitAccessibleTextGetTextAfterOffset;
    iface->get_text_at_offset = webkitAccessibleTextGetTextAtOffset;
    iface->get_text_before_offset = webkitAccessibleTextGetTextBeforeOffset;
    iface->get_character_at_offset = webkitAccessibleTextGetCharacterAtOffset;
    iface->get_caret_offset = webkitAccessibleTextGetCaretOffset;
    iface->get_run_attributes = webkitAccessibleTextGetRunAttributes;
    iface->get_default_attributes = webkitAccessibleTextGetDefaultAttributes;
    iface->get_character_extents = webkitAccessibleTextGetCharacterExtents;
    iface->get_range_extents = webkitAccessibleTextGetRangeExtents;
    iface->get_character_count = webkitAccessibleTextGetCharacterCount;
    iface->get_offset_at_point = webkitAccessibleTextGetOffsetAtPoint;
    iface->get_n_selections = webkitAccessibleTextGetNSelections;
    iface->get_selection = webkitAccessibleTextGetSelection;
    iface->add_selection = webkitAccessibleTextAddSelection;
    iface->remove_selection = webkitAccessibleTextRemoveSelection;
    iface->set_selection = webkitAccessibleTextSetSelection;
    iface->set_caret_offset = webkitAccessibleTextSetCaretOffset;
    iface->get_string_at_offset = webkitAccessibleTextGetStringAtOffset;
}

#endif
