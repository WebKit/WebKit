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
#include "CharacterNames.h"
#include "Editor.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "RenderLayer.h"
#include "RenderText.h"
#include "ScrollbarTheme.h"
#include "SelectionController.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"

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
    , m_wasChangedSinceLastChangeEvent(false)
    , m_lastChangeWasUserEdit(false)
{
}

RenderTextControl::~RenderTextControl()
{
    // The children renderers have already been destroyed by destroyLeftoverChildren
    if (m_innerText)
        m_innerText->detach();
}

void RenderTextControl::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_innerText) {
        RenderBlock* textBlockRenderer = toRenderBlock(m_innerText->renderer());
        RefPtr<RenderStyle> textBlockStyle = createInnerTextStyle(style());
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        textBlockRenderer->style()->setHeight(Length());
        textBlockRenderer->style()->setWidth(Length());
        setInnerTextStyle(textBlockStyle);
    }

    setReplaced(isInline());
}

void RenderTextControl::setInnerTextStyle(PassRefPtr<RenderStyle> style)
{
    if (m_innerText) {
        RefPtr<RenderStyle> textStyle = style;
        m_innerText->renderer()->setStyle(textStyle);
        for (Node* n = m_innerText->firstChild(); n; n = n->traverseNextNode(m_innerText.get())) {
            if (n->renderer())
                n->renderer()->setStyle(textStyle);
        }
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

void RenderTextControl::createSubtreeIfNeeded(TextControlInnerElement* innerBlock)
{
    if (!m_innerText) {
        // Create the text block element
        // For non-search fields, there is no intermediate innerBlock as the shadow node.
        // m_innerText will be the shadow node in that case.        
        RenderStyle* parentStyle = innerBlock ? innerBlock->renderer()->style() : style();
        m_innerText = new TextControlInnerTextElement(document(), innerBlock ? 0 : node());
        m_innerText->attachInnerElement(innerBlock ? innerBlock : node(), createInnerTextStyle(parentStyle), renderArena());
    }
}

int RenderTextControl::textBlockHeight() const
{
    return height() - borderAndPaddingHeight();
}

int RenderTextControl::textBlockWidth() const
{
    return width() - borderAndPaddingWidth() - m_innerText->renderBox()->paddingLeft() - m_innerText->renderBox()->paddingRight();
}

void RenderTextControl::updateFromElement()
{
    updateUserModifyProperty(node(), m_innerText->renderer()->style());
}

void RenderTextControl::setInnerTextValue(const String& innerTextValue)
{
    String value = innerTextValue;
    if (value != text() || !m_innerText->hasChildNodes()) {
        if (value != text()) {
            if (Frame* frame = document()->frame()) {
                frame->editor()->clearUndoRedoOperations();
                
                if (AXObjectCache::accessibilityEnabled())
                    document()->axObjectCache()->postNotification(this, AXObjectCache::AXValueChanged, false);
            }
        }

        ExceptionCode ec = 0;
        m_innerText->setInnerText(value, ec);
        ASSERT(!ec);

        if (value.endsWith("\n") || value.endsWith("\r")) {
            m_innerText->appendChild(new HTMLBRElement(brTag, document()), ec);
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

int RenderTextControl::selectionStart()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selection()->start());
}

int RenderTextControl::selectionEnd()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selection()->end());
}

void RenderTextControl::setSelectionStart(int start)
{
    setSelectionRange(start, max(start, selectionEnd()));
}

void RenderTextControl::setSelectionEnd(int end)
{
    setSelectionRange(min(end, selectionStart()), end);
}

void RenderTextControl::select()
{
    setSelectionRange(0, text().length());
}

void RenderTextControl::setSelectionRange(int start, int end)
{
    end = max(end, 0);
    start = min(max(start, 0), end);

    ASSERT(!document()->childNeedsAndNotInStyleRecalc());

    if (style()->visibility() == HIDDEN || !m_innerText || !m_innerText->renderer() || !m_innerText->renderBox()->height()) {
        cacheSelection(start, end);
        return;
    }
    VisiblePosition startPosition = visiblePositionForIndex(start);
    VisiblePosition endPosition;
    if (start == end)
        endPosition = startPosition;
    else
        endPosition = visiblePositionForIndex(end);

    // startPosition and endPosition can be null position for example when
    // "-webkit-user-select: none" style attribute is specified.
    if (startPosition.isNotNull() && endPosition.isNotNull()) {
        ASSERT(startPosition.deepEquivalent().node()->shadowAncestorNode() == node() && endPosition.deepEquivalent().node()->shadowAncestorNode() == node());
    }
    VisibleSelection newSelection = VisibleSelection(startPosition, endPosition);

    if (Frame* frame = document()->frame())
        frame->selection()->setSelection(newSelection);
}

VisibleSelection RenderTextControl::selection(int start, int end) const
{
    return VisibleSelection(VisiblePosition(m_innerText.get(), start, VP_DEFAULT_AFFINITY),
                            VisiblePosition(m_innerText.get(), end, VP_DEFAULT_AFFINITY));
}

VisiblePosition RenderTextControl::visiblePositionForIndex(int index)
{
    if (index <= 0)
        return VisiblePosition(m_innerText.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(m_innerText.get(), ec);
    ASSERT(!ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    Node* endContainer = it.range()->endContainer(ec);
    ASSERT(!ec);
    int endOffset = it.range()->endOffset(ec);
    ASSERT(!ec);
    return VisiblePosition(endContainer, endOffset, UPSTREAM);
}

int RenderTextControl::indexForVisiblePosition(const VisiblePosition& pos)
{
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != m_innerText)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->setStart(m_innerText.get(), 0, ec);
    ASSERT(!ec);
    range->setEnd(indexPosition.node(), indexPosition.deprecatedEditingOffset(), ec);
    ASSERT(!ec);
    return TextIterator::rangeLength(range.get());
}

void RenderTextControl::subtreeHasChanged()
{
    m_wasChangedSinceLastChangeEvent = true;
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
    if (!m_innerText)
        return "";
 
    Vector<UChar> result;

    for (Node* n = m_innerText.get(); n; n = n->traverseNextNode(m_innerText.get())) {
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
    if (!m_innerText)
        return "";
    Node* firstChild = m_innerText->firstChild();
    if (!firstChild)
        return "";

    RenderObject* renderer = firstChild->renderer();
    if (!renderer)
        return "";

    InlineBox* box = renderer->isText() ? toRenderText(renderer)->firstTextBox() : toRenderBox(renderer)->inlineBoxWrapper();
    if (!box)
        return "";

    Node* breakNode;
    unsigned breakOffset;
    RootInlineBox* line = box->root();
    getNextSoftBreak(line, breakNode, breakOffset);

    Vector<UChar> result;

    for (Node* n = firstChild; n; n = n->traverseNextNode(m_innerText.get())) {
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

void RenderTextControl::calcHeight()
{
    setHeight(m_innerText->renderBox()->borderTop() + m_innerText->renderBox()->borderBottom() +
              m_innerText->renderBox()->paddingTop() + m_innerText->renderBox()->paddingBottom() +
              m_innerText->renderBox()->marginTop() + m_innerText->renderBox()->marginBottom());

    adjustControlHeightBasedOnLineHeight(m_innerText->renderer()->lineHeight(true, true));
    setHeight(height() + borderAndPaddingHeight());

    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (style()->overflowX() == OSCROLL ||  (style()->overflowX() == OAUTO && m_innerText->renderer()->style()->wordWrap() == NormalWordWrap))
        setHeight(height() + scrollbarThickness());

    RenderBlock::calcHeight();
}

void RenderTextControl::hitInnerTextElement(HitTestResult& result, int xPos, int yPos, int tx, int ty)
{
    result.setInnerNode(m_innerText.get());
    result.setInnerNonSharedNode(m_innerText.get());
    result.setLocalPoint(IntPoint(xPos - tx - x() - m_innerText->renderBox()->x(),
                                  yPos - ty - y() - m_innerText->renderBox()->y()));
}

void RenderTextControl::forwardEvent(Event* event)
{
    if (event->type() == eventNames().blurEvent || event->type() == eventNames().focusEvent)
        return;
    m_innerText->defaultEventHandler(event);
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
        
        for (unsigned i = 0; i < sizeof(fontFamiliesWithInvalidCharWidth) / sizeof(fontFamiliesWithInvalidCharWidth[0]); i++)
            fontFamiliesWithInvalidCharWidthMap->add(AtomicString(fontFamiliesWithInvalidCharWidth[i]));
    }

    return !fontFamiliesWithInvalidCharWidthMap->contains(family);
}

float RenderTextControl::getAvgCharWidth(AtomicString family)
{
    if (hasValidAvgCharWidth(family))
        return roundf(style()->font().primaryFont()->avgCharWidth());

    const UChar ch = '0'; 
    return style()->font().floatWidth(TextRun(&ch, 1, false, 0, 0, false, false, false));
}

float RenderTextControl::scaleEmToUnits(int x) const
{
    // This matches the unitsPerEm value for MS Shell Dlg and Courier New from the "head" font table.
    float unitsPerEm = 2048.0f;
    return roundf(style()->font().size() * x / unitsPerEm);
}

void RenderTextControl::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Use average character width. Matches IE.
        AtomicString family = style()->font().family().family();
        m_maxPrefWidth = preferredContentWidth(getAvgCharWidth(family)) + m_innerText->renderBox()->paddingLeft() + m_innerText->renderBox()->paddingRight();
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();

    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false);
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    cacheSelection(selectionStart(), selectionEnd());

    if (Frame* frame = document()->frame()) {
        if (frame->selection()->isRange() && userTriggered)
            node()->dispatchEvent(Event::create(eventNames().selectEvent, true, false));
    }
}

void RenderTextControl::addFocusRingRects(Vector<IntRect>& rects, int tx, int ty)
{
    if (width() && height())
        rects.append(IntRect(tx, ty, width(), height()));
}

HTMLElement* RenderTextControl::innerTextElement() const
{
    return m_innerText.get();
}

void RenderTextControl::updatePlaceholderVisibility(bool placeholderShouldBeVisible, bool placeholderValueChanged)
{
    bool oldPlaceholderVisible = m_placeholderVisible;
    m_placeholderVisible = placeholderShouldBeVisible;
    if (oldPlaceholderVisible != m_placeholderVisible || placeholderValueChanged) {
        // Sets the inner text style to the normal style or :placeholder style.
        setInnerTextStyle(createInnerTextStyle(textBaseStyle()));

        // updateFromElement() of the subclasses updates the text content
        // to the element's value(), placeholder(), or the empty string.
        updateFromElement();
    }
}

} // namespace WebCore
