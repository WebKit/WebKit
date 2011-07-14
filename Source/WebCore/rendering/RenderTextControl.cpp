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
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Position.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "ScrollbarTheme.h"
#include "Text.h"
#include "TextIterator.h"
#include <wtf/text/StringBuilder.h>
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

RenderTextControl::RenderTextControl(Node* node)
    : RenderBlock(node)
    , m_lastChangeWasUserEdit(false)
{
    ASSERT(toTextFormControl(node));
}

RenderTextControl::~RenderTextControl()
{
}

HTMLTextFormControlElement* RenderTextControl::textFormControlElement() const
{
    return static_cast<HTMLTextFormControlElement*>(node());
}

HTMLElement* RenderTextControl::innerTextElement() const
{
    return textFormControlElement()->innerTextElement();
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

void RenderTextControl::setInnerTextValue(const String& value)
{
    bool textIsChanged = value != text();
    if (textIsChanged || !innerTextElement()->hasChildNodes()) {
        if (textIsChanged && document() && AXObjectCache::accessibilityEnabled())
            document()->axObjectCache()->postNotification(this, AXObjectCache::AXValueChanged, false);

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

VisiblePosition RenderTextControl::visiblePositionForIndex(int index) const
{
    if (index <= 0)
        return VisiblePosition(firstPositionInNode(innerTextElement()), DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(innerTextElement(), ec);
    ASSERT(!ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(it.range()->endPosition(), UPSTREAM);
}

static String finishText(StringBuilder& result)
{
    // Remove one trailing newline; there's always one that's collapsed out by rendering.
    size_t size = result.length();
    if (size && result[size - 1] == '\n')
        result.resize(--size);
    return result.toString();
}

String RenderTextControl::text()
{
    HTMLElement* innerText = innerTextElement();
    if (!innerText)
        return emptyString();
 
    StringBuilder result;
    for (Node* node = innerText; node; node = node->traverseNextNode(innerText)) {
        if (node->hasTagName(brTag))
            result.append(newlineCharacter);
        else if (node->isTextNode())
            result.append(static_cast<Text*>(node)->data());
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
        return emptyString();

    RenderBlock* renderer = toRenderBlock(innerText->renderer());
    if (!renderer)
        return emptyString();

    Node* breakNode;
    unsigned breakOffset;
    RootInlineBox* line = renderer->firstRootBox();
    if (!line)
        return emptyString();

    getNextSoftBreak(line, breakNode, breakOffset);

    StringBuilder result;
    for (Node* node = innerText->firstChild(); node; node = node->traverseNextNode(innerText)) {
        if (node->hasTagName(brTag))
            result.append(newlineCharacter);
        else if (node->isTextNode()) {
            String data = static_cast<Text*>(node)->data();
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

void RenderTextControl::hitInnerTextElement(HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    IntPoint adjustedLocation = accumulatedOffset + location();
    HTMLElement* innerText = innerTextElement();
    result.setInnerNode(innerText);
    result.setInnerNonSharedNode(innerText);
    result.setLocalPoint(pointInContainer - toSize(adjustedLocation + innerText->renderBox()->location()));
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
    const String str = String(&ch, 1);
    const Font& font = style()->font();
    TextRun textRun = constructTextRun(this, font, str, style(), TextRun::AllowTrailingExpansion);
    textRun.disableRoundingHacks();
    return font.width(textRun);
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

    LayoutUnit toAdd = borderAndPaddingWidth();

    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false);
}

void RenderTextControl::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset)
{
    if (!size().isEmpty())
        rects.append(LayoutRect(additionalOffset, size()));
}

RenderObject* RenderTextControl::layoutSpecialExcludedChild(bool relayoutChildren)
{
    HTMLElement* placeholder = toTextFormControl(node())->placeholderElement();
    RenderObject* placeholderRenderer = placeholder ? placeholder->renderer() : 0;
    if (!placeholderRenderer)
        return 0;
    if (relayoutChildren)
        placeholderRenderer->setNeedsLayout(true);
    return placeholderRenderer;
}

} // namespace WebCore
