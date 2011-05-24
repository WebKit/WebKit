/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)  
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
#include "RenderTextControl.h"

#include "AXObjectCache.h"
#include "Editor.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLBRElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Position.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "ScrollbarTheme.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include <wtf/unicode/CharacterNames.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Value chosen by observation.  This can be tweaked.
static const int minColorContrastValue = 1300;

static Color disabledTextColor(const Color& textColor, const Color& backgroundColor)
{
    // The explicit check for black is an optimization for the 99% case (black on white).
    // This also means that black on black will turn into grey on black when disabled.
    Color disabledColor;
    if (textColor.rgb() == Color::black || differenceSquared(textColor, Color::white) > differenceSquared(backgroundColor, Color::white))
        disabledColor = textColor.light();
    else
        disabledColor = textColor.dark();
    
    // If there's not very much contrast between the disabled color and the background color,
    // just leave the text color alone.  We don't want to change a good contrast color scheme so that it has really bad contrast.
    // If the the contrast was already poor, then it doesn't do any good to change it to a different poor contrast color scheme.
    if (differenceSquared(disabledColor, backgroundColor) < minColorContrastValue)
        return textColor;
    
    return disabledColor;
}

RenderTextControl::RenderTextControl(Node* node, bool placeholderVisible)
    : RenderBlock(node)
    , m_placeholderVisible(placeholderVisible)
    , m_lastChangeWasUserEdit(false)
{
}

RenderTextControl::~RenderTextControl()
{
}

void RenderTextControl::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    Element* innerText = innerTextElement();
    if (!innerText)
        return;
    RenderBlock* innerTextRenderer = toRenderBlock(innerText->renderer());
    if (innerTextRenderer) {
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        innerTextRenderer->style()->setHeight(Length());
        innerTextRenderer->style()->setWidth(Length());
        innerTextRenderer->setStyle(createInnerTextStyle(style()));
        innerText->setNeedsStyleRecalc();
    }
}

static inline bool updateUserModifyProperty(Node* node, RenderStyle* style)
{
    bool isEnabled = true;
    bool isReadOnlyControl = false;

    if (node->isElementNode()) {
        Element* element = static_cast<Element*>(node);
        isEnabled = element->isEnabledFormControl();
        isReadOnlyControl = element->isReadOnlyFormControl();
    }

    style->setUserModify((isReadOnlyControl || !isEnabled) ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    return !isEnabled;
}

void RenderTextControl::adjustInnerTextStyle(const RenderStyle* startStyle, RenderStyle* textBlockStyle) const
{
    // The inner block, if present, always has its direction set to LTR,
    // so we need to inherit the direction from the element.
    textBlockStyle->setDirection(style()->direction());

    bool disabled = updateUserModifyProperty(node(), textBlockStyle);
    if (disabled)
        textBlockStyle->setColor(disabledTextColor(textBlockStyle->visitedDependentColor(CSSPropertyColor), startStyle->visitedDependentColor(CSSPropertyBackgroundColor)));
}

int RenderTextControl::textBlockHeight() const
{
    return height() - borderAndPaddingHeight();
}

int RenderTextControl::textBlockWidth() const
{
    Element* innerText = innerTextElement();
    ASSERT(innerText);
    return width() - borderAndPaddingWidth() - innerText->renderBox()->paddingLeft() - innerText->renderBox()->paddingRight();
}

void RenderTextControl::updateFromElement()
{
    Element* innerText = innerTextElement();
    if (innerText)
        updateUserModifyProperty(node(), innerText->renderer()->style());
}

void RenderTextControl::setInnerTextValue(const String& innerTextValue)
{
    String value = innerTextValue;
    if (value != text() || !innerTextElement()->hasChildNodes()) {
        if (value != text()) {
            if (Frame* frame = this->frame()) {
                frame->editor()->clearUndoRedoOperations();
                
                if (AXObjectCache::accessibilityEnabled())
                    document()->axObjectCache()->postNotification(this, AXObjectCache::AXValueChanged, false);
            }
        }

        ExceptionCode ec = 0;
        innerTextElement()->setInnerText(value, ec);
        ASSERT(!ec);

        if (value.endsWith("\n") || value.endsWith("\r")) {
            innerTextElement()->appendChild(HTMLBRElement::create(document()), ec);
            ASSERT(!ec);
        }

        // We set m_lastChangeWasUserEdit to false since this change was not explicitly made by the user (say, via typing on the keyboard), see <rdar://problem/5359921>.
        m_lastChangeWasUserEdit = false;
    }

    static_cast<Element*>(node())->setFormControlValueMatchesRenderer(true);
}

void RenderTextControl::setLastChangeWasUserEdit(bool lastChangeWasUserEdit)
{
    m_lastChangeWasUserEdit = lastChangeWasUserEdit;
    document()->setIgnoreAutofocus(lastChangeWasUserEdit);
}

int RenderTextControl::selectionStart() const
{
    Frame* frame = this->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selection()->start());
}

int RenderTextControl::selectionEnd() const
{
    Frame* frame = this->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selection()->end());
}

bool RenderTextControl::hasVisibleTextArea() const
{
    HTMLElement* innerText = innerTextElement();
    return style()->visibility() == HIDDEN || !innerText || !innerText->renderer() || !innerText->renderBox()->height();
}

void setSelectionRange(Node* node, int start, int end)
{
    ASSERT(node);
    node->document()->updateLayoutIgnorePendingStylesheets();

    if (!node->renderer() || !node->renderer()->isTextControl())
        return;

    end = max(end, 0);
    start = min(max(start, 0), end);

    RenderTextControl* control = toRenderTextControl(node->renderer());

    if (control->hasVisibleTextArea()) {
        control->cacheSelection(start, end);
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
        ASSERT(startPosition.deepEquivalent().deprecatedNode()->shadowAncestorNode() == node && endPosition.deepEquivalent().deprecatedNode()->shadowAncestorNode() == node);
    }
    VisibleSelection newSelection = VisibleSelection(startPosition, endPosition);

    if (Frame* frame = node->document()->frame())
        frame->selection()->setSelection(newSelection);
}

bool RenderTextControl::isSelectableElement(Node* node) const
{
    if (!node)
        return false;

    HTMLElement* innerText = innerTextElement();
    if (!innerText)
        return false;
    
    if (node->rootEditableElement() == innerText)
        return true;
    
    if (!innerText->contains(node))
        return false;
    
    Node* shadowAncestor = node->shadowAncestorNode();
    return shadowAncestor && (shadowAncestor->hasTagName(textareaTag)
        || (shadowAncestor->hasTagName(inputTag) && static_cast<HTMLInputElement*>(shadowAncestor)->isTextField()));
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

PassRefPtr<Range> RenderTextControl::selection(int start, int end) const
{
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

VisiblePosition RenderTextControl::visiblePositionForIndex(int index) const
{
    if (index <= 0)
        return VisiblePosition(Position(innerTextElement(), 0, Position::PositionIsOffsetInAnchor), DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(innerTextElement(), ec);
    ASSERT(!ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    Node* endContainer = it.range()->endContainer(ec);
    ASSERT(!ec);
    int endOffset = it.range()->endOffset(ec);
    ASSERT(!ec);
    return VisiblePosition(Position(endContainer, endOffset, Position::PositionIsOffsetInAnchor), UPSTREAM);
}

int RenderTextControl::indexForVisiblePosition(const VisiblePosition& pos) const
{
    Position indexPosition = pos.deepEquivalent();
    if (!isSelectableElement(indexPosition.deprecatedNode()))
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->setStart(innerTextElement(), 0, ec);
    ASSERT(!ec);
    range->setEnd(indexPosition.deprecatedNode(), indexPosition.deprecatedEditingOffset(), ec);
    ASSERT(!ec);
    return TextIterator::rangeLength(range.get());
}

void RenderTextControl::subtreeHasChanged()
{
    m_lastChangeWasUserEdit = true;
}

String RenderTextControl::finishText(Vector<UChar>& result) const
{
    // Remove one trailing newline; there's always one that's collapsed out by rendering.
    size_t size = result.size();
    if (size && result[size - 1] == '\n')
        result.shrink(--size);

    return String::adopt(result);
}

String RenderTextControl::text()
{
    HTMLElement* innerText = innerTextElement();
    if (!innerText)
        return "";
 
    Vector<UChar> result;

    for (Node* n = innerText; n; n = n->traverseNextNode(innerText)) {
        if (n->hasTagName(brTag))
            result.append(&newlineCharacter, 1);
        else if (n->isTextNode()) {
            String data = static_cast<Text*>(n)->data();
            result.append(data.characters(), data.length());
        }
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

String RenderTextControl::textWithHardLineBreaks()
{
    HTMLElement* innerText = innerTextElement();
    if (!innerText)
        return "";

    RenderBlock* renderer = toRenderBlock(innerText->renderer());
    if (!renderer)
        return "";

    Node* breakNode;
    unsigned breakOffset;
    RootInlineBox* line = renderer->firstRootBox();
    if (!line)
        return "";

    getNextSoftBreak(line, breakNode, breakOffset);

    Vector<UChar> result;

    for (Node* n = innerText->firstChild(); n; n = n->traverseNextNode(innerText)) {
        if (n->hasTagName(brTag))
            result.append(&newlineCharacter, 1);
        else if (n->isTextNode()) {
            Text* text = static_cast<Text*>(n);
            String data = text->data();
            unsigned length = data.length();
            unsigned position = 0;
            while (breakNode == n && breakOffset <= length) {
                if (breakOffset > position) {
                    result.append(data.characters() + position, breakOffset - position);
                    position = breakOffset;
                    result.append(&newlineCharacter, 1);
                }
                getNextSoftBreak(line, breakNode, breakOffset);
            }
            result.append(data.characters() + position, length - position);
        }
        while (breakNode == n)
            getNextSoftBreak(line, breakNode, breakOffset);
    }

    return finishText(result);
}

int RenderTextControl::scrollbarThickness() const
{
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    return ScrollbarTheme::nativeTheme()->scrollbarThickness();
}

void RenderTextControl::computeLogicalHeight()
{
    HTMLElement* innerText = innerTextElement();
    ASSERT(innerText);
    RenderBox* innerTextRenderBox = innerText->renderBox();

    setHeight(innerTextRenderBox->borderTop() + innerTextRenderBox->borderBottom() +
              innerTextRenderBox->paddingTop() + innerTextRenderBox->paddingBottom() +
              innerTextRenderBox->marginTop() + innerTextRenderBox->marginBottom());

    adjustControlHeightBasedOnLineHeight(innerText->renderBox()->lineHeight(true, HorizontalLine, PositionOfInteriorLineBoxes));
    setHeight(height() + borderAndPaddingHeight());

    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (style()->overflowX() == OSCROLL ||  (style()->overflowX() == OAUTO && innerText->renderer()->style()->wordWrap() == NormalWordWrap))
        setHeight(height() + scrollbarThickness());

    RenderBlock::computeLogicalHeight();
}

void RenderTextControl::hitInnerTextElement(HitTestResult& result, const IntPoint& pointInContainer, int tx, int ty)
{
    HTMLElement* innerText = innerTextElement();
    result.setInnerNode(innerText);
    result.setInnerNonSharedNode(innerText);
    result.setLocalPoint(pointInContainer -
        IntSize(tx + x() + innerText->renderBox()->x(), ty + y() + innerText->renderBox()->y()));
}

void RenderTextControl::forwardEvent(Event* event)
{
    if (event->type() == eventNames().blurEvent || event->type() == eventNames().focusEvent)
        return;
    innerTextElement()->defaultEventHandler(event);
}

static const char* fontFamiliesWithInvalidCharWidth[] = {
    "American Typewriter",
    "Arial Hebrew",
    "Chalkboard",
    "Cochin",
    "Corsiva Hebrew",
    "Courier",
    "Euphemia UCAS",
    "Geneva",
    "Gill Sans",
    "Hei",
    "Helvetica",
    "Hoefler Text",
    "InaiMathi",
    "Kai",
    "Lucida Grande",
    "Marker Felt",
    "Monaco",
    "Mshtakan",
    "New Peninim MT",
    "Osaka",
    "Raanana",
    "STHeiti",
    "Symbol",
    "Times",
    "Apple Braille",
    "Apple LiGothic",
    "Apple LiSung",
    "Apple Symbols",
    "AppleGothic",
    "AppleMyungjo",
    "#GungSeo",
    "#HeadLineA",
    "#PCMyungjo",
    "#PilGi",
};

// For font families where any of the fonts don't have a valid entry in the OS/2 table
// for avgCharWidth, fallback to the legacy webkit behavior of getting the avgCharWidth
// from the width of a '0'. This only seems to apply to a fixed number of Mac fonts,
// but, in order to get similar rendering across platforms, we do this check for
// all platforms.
bool RenderTextControl::hasValidAvgCharWidth(AtomicString family)
{
    static HashSet<AtomicString>* fontFamiliesWithInvalidCharWidthMap = 0;

    if (!fontFamiliesWithInvalidCharWidthMap) {
        fontFamiliesWithInvalidCharWidthMap = new HashSet<AtomicString>;

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontFamiliesWithInvalidCharWidth); ++i)
            fontFamiliesWithInvalidCharWidthMap->add(AtomicString(fontFamiliesWithInvalidCharWidth[i]));
    }

    return !fontFamiliesWithInvalidCharWidthMap->contains(family);
}

float RenderTextControl::getAvgCharWidth(AtomicString family)
{
    if (hasValidAvgCharWidth(family))
        return roundf(style()->font().primaryFont()->avgCharWidth());

    const UChar ch = '0';
    const Font& font = style()->font();
    return font.width(constructTextRun(this, font, String(&ch, 1), style(), TextRun::AllowTrailingExpansion));
}

float RenderTextControl::scaleEmToUnits(int x) const
{
    // This matches the unitsPerEm value for MS Shell Dlg and Courier New from the "head" font table.
    float unitsPerEm = 2048.0f;
    return roundf(style()->font().size() * x / unitsPerEm);
}

void RenderTextControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(style()->width().value());
    else {
        // Use average character width. Matches IE.
        AtomicString family = style()->font().family().family();
        RenderBox* innerTextRenderBox = innerTextElement()->renderBox();
        m_maxPreferredLogicalWidth = preferredContentWidth(getAvgCharWidth(family)) + innerTextRenderBox->paddingLeft() + innerTextRenderBox->paddingRight();
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPreferredLogicalWidth = 0;
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();

    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false);
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    cacheSelection(selectionStart(), selectionEnd());

    if (Frame* frame = this->frame()) {
        if (frame->selection()->isRange() && userTriggered)
            node()->dispatchEvent(Event::create(eventNames().selectEvent, true, false));
    }
}

void RenderTextControl::addFocusRingRects(Vector<IntRect>& rects, int tx, int ty)
{
    if (width() && height())
        rects.append(IntRect(tx, ty, width(), height()));
}

void RenderTextControl::updatePlaceholderVisibility(bool placeholderShouldBeVisible, bool placeholderValueChanged)
{
    bool oldPlaceholderVisible = m_placeholderVisible;
    m_placeholderVisible = placeholderShouldBeVisible;
    if (oldPlaceholderVisible != m_placeholderVisible || placeholderValueChanged)
        repaint();
}

void RenderTextControl::paintPlaceholder(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE)
        return;
    
    IntRect clipRect(tx + borderLeft(), ty + borderTop(), width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop());
    if (clipRect.isEmpty())
        return;
    
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    paintInfo.context->clip(clipRect);
    
    RefPtr<RenderStyle> placeholderStyle = getCachedPseudoStyle(INPUT_PLACEHOLDER);
    if (!placeholderStyle)
        placeholderStyle = style();
    
    paintInfo.context->setFillColor(placeholderStyle->visitedDependentColor(CSSPropertyColor), placeholderStyle->colorSpace());
    
    String placeholderText = static_cast<HTMLTextFormControlElement*>(node())->strippedPlaceholder();
    TextRun textRun(placeholderText.characters(), placeholderText.length(), false, 0, 0, TextRun::AllowTrailingExpansion, placeholderStyle->direction(), placeholderStyle->unicodeBidi() == Override);
    
    RenderBox* textRenderer = innerTextElement() ? innerTextElement()->renderBox() : 0;
    if (textRenderer) {
        IntPoint textPoint;
        textPoint.setY(ty + textBlockInsetTop() + placeholderStyle->fontMetrics().ascent());
        int styleTextIndent = placeholderStyle->textIndent().isFixed() ? placeholderStyle->textIndent().calcMinValue(0) : 0;
        if (placeholderStyle->isLeftToRightDirection())
            textPoint.setX(tx + styleTextIndent + textBlockInsetLeft());
        else
            textPoint.setX(tx + width() - textBlockInsetRight() - styleTextIndent - style()->font().width(textRun));
        
        paintInfo.context->drawBidiText(placeholderStyle->font(), textRun, textPoint);
    }
}

void RenderTextControl::paintObject(PaintInfo& paintInfo, int tx, int ty)
{    
    if (m_placeholderVisible && paintInfo.phase == PaintPhaseForeground)
        paintPlaceholder(paintInfo, tx, ty);
    
    RenderBlock::paintObject(paintInfo, tx, ty);
}

} // namespace WebCore
