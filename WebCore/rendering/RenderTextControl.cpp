/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "CharacterNames.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLBRElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "ScrollbarTheme.h"
#include "SearchPopupMenu.h"
#include "SelectionController.h"
#include "Settings.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <math.h>

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

RenderTextControl::RenderTextControl(Node* node, bool multiLine)
    : RenderBlock(node)
    , m_dirty(false)
    , m_multiLine(multiLine)
    , m_placeholderVisible(false)
    , m_userEdited(false)
    , m_shouldDrawCapsLockIndicator(false)
    , m_searchPopup(0)
    , m_searchPopupIsVisible(false)
    , m_searchEventTimer(this, &RenderTextControl::searchEventTimerFired)
{
}

RenderTextControl::~RenderTextControl()
{
    if (m_searchPopup) {
        m_searchPopup->disconnectClient();
        m_searchPopup = 0;
    }
    if (m_multiLine && node())
        static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
    // The children renderers have already been destroyed by destroyLeftoverChildren
    if (m_innerBlock)
        m_innerBlock->detach();
    else if (m_innerText)
        m_innerText->detach();
}

void RenderTextControl::styleDidChange(RenderStyle::Diff diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (m_innerBlock) {
        // We may have set the width and the height in the old style in layout(). Reset them now to avoid
        // getting a spurious layout hint.
        m_innerBlock->renderer()->style()->setHeight(Length());
        m_innerBlock->renderer()->style()->setWidth(Length());
        m_innerBlock->renderer()->setStyle(createInnerBlockStyle(style()));
    }

    if (m_innerText) {
        RenderBlock* textBlockRenderer = static_cast<RenderBlock*>(m_innerText->renderer());
        RefPtr<RenderStyle> textBlockStyle = createInnerTextStyle(style());
        // We may have set the width and the height in the old style in layout(). Reset them now to avoid
        // getting a spurious layout hint.
        textBlockRenderer->style()->setHeight(Length());
        textBlockRenderer->style()->setWidth(Length());
        textBlockRenderer->setStyle(textBlockStyle);
        for (Node* n = m_innerText->firstChild(); n; n = n->traverseNextNode(m_innerText.get())) {
            if (n->renderer())
                n->renderer()->setStyle(textBlockStyle);
        }
    }
    if (m_resultsButton && m_resultsButton->renderer())
        m_resultsButton->renderer()->setStyle(createResultsButtonStyle(style()));

    if (m_cancelButton && m_cancelButton->renderer())
        m_cancelButton->renderer()->setStyle(createCancelButtonStyle(style()));

    setHasOverflowClip(false);
    setReplaced(isInline());
}

PassRefPtr<RenderStyle> RenderTextControl::createInnerBlockStyle(const RenderStyle* startStyle)
{
    RefPtr<RenderStyle> innerBlockStyle = RenderStyle::create();

    innerBlockStyle->inheritFrom(startStyle);
    innerBlockStyle->setDisplay(BLOCK);
    innerBlockStyle->setDirection(LTR);
    // We don't want the shadow dom to be editable, so we set this block to read-only in case the input itself is editable.
    innerBlockStyle->setUserModify(READ_ONLY);

    return innerBlockStyle.release();
}

PassRefPtr<RenderStyle> RenderTextControl::createInnerTextStyle(const RenderStyle* startStyle)
{
    HTMLFormControlElement* element = static_cast<HTMLFormControlElement*>(node());
    bool placeholderShouldBeVisible = !m_multiLine && static_cast<HTMLInputElement*>(element)->placeholderShouldBeVisible();

    RefPtr<RenderStyle> textBlockStyle;
    if (placeholderShouldBeVisible) {
        RenderStyle* pseudoStyle = getCachedPseudoStyle(RenderStyle::INPUT_PLACEHOLDER);
        textBlockStyle = RenderStyle::clone(pseudoStyle);
    } else {
        textBlockStyle = RenderStyle::create();   
        textBlockStyle->inheritFrom(startStyle);
    }
    
    // The inner block, if present, always has its direction set to LTR,
    // so we need to inherit the direction from the element.
    textBlockStyle->setDirection(style()->direction());
    textBlockStyle->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    if (m_innerBlock)
        textBlockStyle->setDisplay(INLINE_BLOCK);
    else
        textBlockStyle->setDisplay(BLOCK);

    if (m_multiLine) {
        // Forward overflow properties.
        textBlockStyle->setOverflowX(startStyle->overflowX() == OVISIBLE ? OAUTO : startStyle->overflowX());
        textBlockStyle->setOverflowY(startStyle->overflowY() == OVISIBLE ? OAUTO : startStyle->overflowY());

        // Set word wrap property based on wrap attribute.
        if (!static_cast<HTMLTextAreaElement*>(element)->shouldWrapText()) {
            textBlockStyle->setWhiteSpace(PRE);
            textBlockStyle->setWordWrap(NormalWordWrap);
        } else {
            textBlockStyle->setWhiteSpace(PRE_WRAP);
            textBlockStyle->setWordWrap(BreakWordWrap);
        }
    } else {
        textBlockStyle->setWhiteSpace(PRE);
        textBlockStyle->setWordWrap(NormalWordWrap);
        textBlockStyle->setOverflowX(OHIDDEN);
        textBlockStyle->setOverflowY(OHIDDEN);
        
        // Do not allow line-height to be smaller than our default.
        if (textBlockStyle->font().lineSpacing() > lineHeight(true, true))
            textBlockStyle->setLineHeight(Length(-100.0f, Percent));
    }

    if (!m_multiLine) {
        // We're adding one extra pixel of padding to match WinIE.
        textBlockStyle->setPaddingLeft(Length(1, Fixed));
        textBlockStyle->setPaddingRight(Length(1, Fixed));
    } else {
        // We're adding three extra pixels of padding to line textareas up with text fields.
        textBlockStyle->setPaddingLeft(Length(3, Fixed));
        textBlockStyle->setPaddingRight(Length(3, Fixed));
    }
    
    // When the placeholder is going to be displayed, temporarily override the text security to be "none".
    // After this, updateFromElement will immediately update the text displayed.
    // When the placeholder is no longer visible, updatePlaceholderVisiblity will reset the style, 
    // and the text security mode will be set back to the computed value correctly.
    if (!m_multiLine && static_cast<HTMLInputElement*>(element)->placeholderShouldBeVisible())
        textBlockStyle->setTextSecurity(TSNONE);

    if (!element->isEnabled())
        textBlockStyle->setColor(disabledTextColor(textBlockStyle->color(), startStyle->backgroundColor()));

    return textBlockStyle.release();
}

PassRefPtr<RenderStyle> RenderTextControl::createResultsButtonStyle(const RenderStyle* startStyle)
{
    ASSERT(!m_multiLine);
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    RefPtr<RenderStyle> resultsBlockStyle;
    if (input->maxResults() < 0)
        resultsBlockStyle = getCachedPseudoStyle(RenderStyle::SEARCH_DECORATION);
    else if (!input->maxResults())
        resultsBlockStyle = getCachedPseudoStyle(RenderStyle::SEARCH_RESULTS_DECORATION);
    else
        resultsBlockStyle = getCachedPseudoStyle(RenderStyle::SEARCH_RESULTS_BUTTON);

    if (!resultsBlockStyle)
        resultsBlockStyle = RenderStyle::create();

    if (startStyle)
        resultsBlockStyle->inheritFrom(startStyle);

    return resultsBlockStyle.release();
}

PassRefPtr<RenderStyle> RenderTextControl::createCancelButtonStyle(const RenderStyle* startStyle)
{
    RefPtr<RenderStyle> cancelBlockStyle;
    
    if (RefPtr<RenderStyle> pseudoStyle = getCachedPseudoStyle(RenderStyle::SEARCH_CANCEL_BUTTON))
        // We may be sharing style with another search field, but we must not share the cancel button style.
        cancelBlockStyle = RenderStyle::clone(pseudoStyle.get());
    else
        cancelBlockStyle = RenderStyle::create();

    if (startStyle)
        cancelBlockStyle->inheritFrom(startStyle);

    updateCancelButtonVisibility(cancelBlockStyle.get());

    return cancelBlockStyle.release();
}

void RenderTextControl::createSubtreeIfNeeded()
{
    bool isSearchField = !m_multiLine && static_cast<HTMLInputElement*>(node())->isSearchField();
    if (isSearchField && !m_innerBlock) {
        // Create the inner block element
        m_innerBlock = new TextControlInnerElement(document(), node());
        m_innerBlock->attachInnerElement(node(), createInnerBlockStyle(style()), renderArena());
    }
    if (isSearchField && !m_resultsButton) {
        // Create the search results button element
        m_resultsButton = new SearchFieldResultsButtonElement(document());
        m_resultsButton->attachInnerElement(m_innerBlock.get(), createResultsButtonStyle(m_innerBlock->renderer()->style()), renderArena());
    }
    if (!m_innerText) {
        // Create the text block element
        // For non-search fields, there is no intermediate m_innerBlock as the shadow node.
        // m_innerText will be the shadow node in that case.
        
        RenderStyle* parentStyle = style();
        if (m_innerBlock)
            parentStyle = m_innerBlock->renderer()->style();
        m_innerText = new TextControlInnerTextElement(document(), m_innerBlock ? 0 : node());
        m_innerText->attachInnerElement(m_innerBlock ? m_innerBlock.get() : node(), createInnerTextStyle(parentStyle), renderArena());
    }
    if (isSearchField && !m_cancelButton) {
        // Create the cancel button element
        m_cancelButton = new SearchFieldCancelButtonElement(document());
        m_cancelButton->attachInnerElement(m_innerBlock.get(), createCancelButtonStyle(m_innerBlock->renderer()->style()), renderArena());
    }
}

void RenderTextControl::updateFromElement()
{
    HTMLFormControlElement* element = static_cast<HTMLFormControlElement*>(node());

    bool placeholderShouldBeVisible = !m_multiLine && static_cast<HTMLInputElement*>(element)->placeholderShouldBeVisible();
    bool placeholderVisibilityShouldChange = m_placeholderVisible != placeholderShouldBeVisible;
    m_placeholderVisible = placeholderShouldBeVisible;

    createSubtreeIfNeeded();

    if (m_cancelButton && m_cancelButton->renderer())
        updateCancelButtonVisibility(m_cancelButton->renderer()->style());

    m_innerText->renderer()->style()->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    
    if (m_placeholderVisible) {
        ExceptionCode ec;
        m_innerText->setInnerText(static_cast<HTMLInputElement*>(element)->getAttribute(placeholderAttr), ec);
    } else if (!element->valueMatchesRenderer() || m_multiLine || placeholderVisibilityShouldChange) {
        String value;
        if (m_multiLine)
            value = static_cast<HTMLTextAreaElement*>(element)->value();
        else
            value = static_cast<HTMLInputElement*>(element)->value();
        if (value.isNull())
            value = "";
        else
            value = value.replace('\\', backslashAsCurrencySymbol());
        if (value != text() || !m_innerText->hasChildNodes()) {
            if (value != text()) {
                if (Frame* frame = document()->frame())
                    frame->editor()->clearUndoRedoOperations();
            }
            ExceptionCode ec = 0;
            m_innerText->setInnerText(value, ec);
            if (value.endsWith("\n") || value.endsWith("\r"))
                m_innerText->appendChild(new HTMLBRElement(brTag, document()), ec);
            m_dirty = false;
            m_userEdited = false;
        }
        element->setValueMatchesRenderer();
    }

    if (m_searchPopupIsVisible)
        m_searchPopup->updateFromElement();
}

void RenderTextControl::setUserEdited(bool isUserEdited)
{
    m_userEdited = isUserEdited;
    document()->setIgnoreAutofocus(isUserEdited);
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

    document()->updateLayout();

    if (style()->visibility() == HIDDEN || !m_innerText || !m_innerText->renderer() || !m_innerText->renderer()->height()) {
        if (m_multiLine)
            static_cast<HTMLTextAreaElement*>(node())->cacheSelection(start, end);
        else
            static_cast<HTMLInputElement*>(node())->cacheSelection(start, end);
        return;
    }
    VisiblePosition startPosition = visiblePositionForIndex(start);
    VisiblePosition endPosition;
    if (start == end)
        endPosition = startPosition;
    else
        endPosition = visiblePositionForIndex(end);

    ASSERT(startPosition.isNotNull() && endPosition.isNotNull());
    ASSERT(startPosition.deepEquivalent().node()->shadowAncestorNode() == node() && endPosition.deepEquivalent().node()->shadowAncestorNode() == node());

    Selection newSelection = Selection(startPosition, endPosition);

    if (Frame* frame = document()->frame())
        frame->selection()->setSelection(newSelection);

    // FIXME: Granularity is stored separately on the frame, but also in the selection controller.
    // The granularity in the selection controller should be used, and then this line of code would not be needed.
    if (Frame* frame = document()->frame())
        frame->setSelectionGranularity(CharacterGranularity);
}

Selection RenderTextControl::selection(int start, int end) const
{
    return Selection(VisiblePosition(m_innerText.get(), start, VP_DEFAULT_AFFINITY),
                     VisiblePosition(m_innerText.get(), end, VP_DEFAULT_AFFINITY));
}

VisiblePosition RenderTextControl::visiblePositionForIndex(int index)
{
    if (index <= 0)
        return VisiblePosition(m_innerText.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(m_innerText.get(), ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(it.range()->endContainer(ec), it.range()->endOffset(ec), UPSTREAM);
}

int RenderTextControl::indexForVisiblePosition(const VisiblePosition& pos)
{
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != m_innerText)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = Range::create(document());
    range->setStart(m_innerText.get(), 0, ec);
    range->setEnd(indexPosition.node(), indexPosition.offset(), ec);
    return TextIterator::rangeLength(range.get());
}

void RenderTextControl::updateCancelButtonVisibility(RenderStyle* style)
{
    ASSERT(!m_multiLine);
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (input->value().isEmpty())
        style->setVisibility(HIDDEN);
    else
        style->setVisibility(VISIBLE);
}

void RenderTextControl::subtreeHasChanged()
{
    bool wasDirty = m_dirty;
    m_dirty = true;
    m_userEdited = true;
    HTMLFormControlElement* element = static_cast<HTMLFormControlElement*>(node());
    if (m_multiLine) {
        element->setValueMatchesRenderer(false);
        if (element->focused())
            if (Frame* frame = document()->frame())
                frame->textDidChangeInTextArea(element);
    } else {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        input->setValueFromRenderer(input->constrainValue(text()));
        if (m_cancelButton && m_cancelButton->renderer())
            updateCancelButtonVisibility(m_cancelButton->renderer()->style());

        // If the incremental attribute is set, then dispatch the search event
        if (!input->getAttribute(incrementalAttr).isNull())
            startSearchEventTimer();

        if (!wasDirty) {
            if (input->focused())
                if (Frame* frame = document()->frame())
                    frame->textFieldDidBeginEditing(input);
        }
        if (input->focused())
            if (Frame* frame = document()->frame())
                frame->textDidChangeInTextField(input);
    }
}

String RenderTextControl::finishText(Vector<UChar>& result) const
{
    // Remove one trailing newline; there's always one that's collapsed out by rendering.
    size_t size = result.size();
    if (size && result[size - 1] == '\n')
        result.shrink(--size);

    // Convert backslash to currency symbol.
    UChar symbol = backslashAsCurrencySymbol();
    if (symbol != '\\') {
        for (size_t i = 0; i < size; ++i) {
            if (result[i] == '\\')
                result[i] = symbol;
        }
    }

    return String::adopt(result);
}

HTMLElement* RenderTextControl::innerTextElement() const
{
    return m_innerText.get();
}

String RenderTextControl::text()
{
    if (!m_innerText)
        return "";
 
    Frame* frame = document()->frame();
    Text* compositionNode = frame ? frame->editor()->compositionNode() : 0;

    Vector<UChar> result;

    for (Node* n = m_innerText.get(); n; n = n->traverseNextNode(m_innerText.get())) {
        if (n->isTextNode()) {
            Text* text = static_cast<Text*>(n);
            String data = text->data();
            unsigned length = data.length();
            if (text != compositionNode)
                result.append(data.characters(), length);
            else {
                unsigned compositionStart = min(frame->editor()->compositionStart(), length);
                unsigned compositionEnd = min(max(compositionStart, frame->editor()->compositionEnd()), length);
                result.append(data.characters(), compositionStart);
                result.append(data.characters() + compositionEnd, length - compositionEnd);
            }
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
}

String RenderTextControl::textWithHardLineBreaks()
{
    if (!m_innerText)
        return "";
    Node* firstChild = m_innerText->firstChild();
    if (!firstChild)
        return "";

    document()->updateLayout();

    RenderObject* renderer = firstChild->renderer();
    if (!renderer)
        return "";

    InlineBox* box = renderer->isText() ? static_cast<RenderText*>(renderer)->firstTextBox() : renderer->inlineBoxWrapper();
    if (!box)
        return "";

    Frame* frame = document()->frame();
    Text* compositionNode = frame ? frame->editor()->compositionNode() : 0;

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
            unsigned compositionStart = (text == compositionNode)
                ? min(frame->editor()->compositionStart(), length) : 0;
            unsigned compositionEnd = (text == compositionNode)
                ? min(max(compositionStart, frame->editor()->compositionEnd()), length) : 0;
            unsigned position = 0;
            while (breakNode == n && breakOffset < compositionStart) {
                result.append(data.characters() + position, breakOffset - position);
                position = breakOffset;
                result.append(&newlineCharacter, 1);
                getNextSoftBreak(line, breakNode, breakOffset);
            }
            result.append(data.characters() + position, compositionStart - position);
            position = compositionEnd;
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

void RenderTextControl::calcHeight()
{
    int rows = 1;
    if (m_multiLine)
        rows = static_cast<HTMLTextAreaElement*>(node())->rows();

    int line = m_innerText->renderer()->lineHeight(true, true);
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom();

    int innerToAdd = m_innerText->renderer()->borderTop() + m_innerText->renderer()->borderBottom() +
                     m_innerText->renderer()->paddingTop() + m_innerText->renderer()->paddingBottom() +
                     m_innerText->renderer()->marginTop() + m_innerText->renderer()->marginBottom();
    
    if (m_resultsButton && m_resultsButton->renderer()) {
        static_cast<RenderBlock*>(m_resultsButton->renderer())->calcHeight();
        innerToAdd = max(innerToAdd,
                         m_resultsButton->renderer()->borderTop() + m_resultsButton->renderer()->borderBottom() +
                         m_resultsButton->renderer()->paddingTop() + m_resultsButton->renderer()->paddingBottom() +
                         m_resultsButton->renderer()->marginTop() + m_resultsButton->renderer()->marginBottom());
        line = max(line, m_resultsButton->renderer()->height());
    }
    if (m_cancelButton && m_cancelButton->renderer()) {
        static_cast<RenderBlock*>(m_cancelButton->renderer())->calcHeight();
        innerToAdd = max(innerToAdd,
                         m_cancelButton->renderer()->borderTop() + m_cancelButton->renderer()->borderBottom() +
                         m_cancelButton->renderer()->paddingTop() + m_cancelButton->renderer()->paddingBottom() +
                         m_cancelButton->renderer()->marginTop() + m_cancelButton->renderer()->marginBottom());
        line = max(line, m_cancelButton->renderer()->height());
    }
    toAdd += innerToAdd;

    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    int scrollbarSize = 0;
    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (m_innerText->renderer()->style()->overflowX() == OSCROLL ||  (m_innerText->renderer()->style()->overflowX() == OAUTO && m_innerText->renderer()->style()->wordWrap() == NormalWordWrap))
        scrollbarSize = ScrollbarTheme::nativeTheme()->scrollbarThickness();

    m_height = line * rows + toAdd + scrollbarSize;

    RenderBlock::calcHeight();
}

int RenderTextControl::baselinePosition(bool b, bool isRootLineBox) const
{
    if (m_multiLine)
        return height() + marginTop() + marginBottom();
    return RenderBlock::baselinePosition(b, isRootLineBox);
}

bool RenderTextControl::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    // If we're within the text control, we want to act as if we've hit the inner text block element, in case the point
    // was on the control but not on the inner element (see Radar 4617841).

    // In a search field, we want to act as if we've hit the results block if we're to the left of the inner text block,
    // and act as if we've hit the close block if we're to the right of the inner text block.

    if (RenderBlock::nodeAtPoint(request, result, x, y, tx, ty, hitTestAction) &&
        (result.innerNode() == element() || result.innerNode() == m_innerBlock)) {
        IntPoint localPoint = IntPoint(x - tx - m_x, y - ty - m_y);
        if (m_innerBlock) {
            int textLeft = tx + m_x + m_innerBlock->renderer()->xPos() + m_innerText->renderer()->xPos();
            int textRight = textLeft + m_innerText->renderer()->width();
            if (m_resultsButton && m_resultsButton->renderer() && x < textLeft) {
                result.setInnerNode(m_resultsButton.get());
                result.setLocalPoint(IntPoint(localPoint.x() - m_innerText->renderer()->xPos() - m_innerBlock->renderer()->xPos() - m_resultsButton->renderer()->xPos(),
                                              localPoint.y() - m_innerText->renderer()->yPos() - m_innerBlock->renderer()->yPos() - m_resultsButton->renderer()->yPos()));
                return true;
            } 
            if (m_cancelButton && m_cancelButton->renderer() && x > textRight) {
                result.setInnerNode(m_cancelButton.get());
                result.setLocalPoint(IntPoint(localPoint.x() - m_innerText->renderer()->xPos() - m_innerBlock->renderer()->xPos() - m_cancelButton->renderer()->xPos(),
                                              localPoint.y() - m_innerText->renderer()->yPos() - m_innerBlock->renderer()->yPos() - m_cancelButton->renderer()->yPos()));
                return true;
            }
        }
        
        // Hit the inner text block.
        result.setInnerNode(m_innerText.get());
        result.setLocalPoint(IntPoint(localPoint.x() - m_innerText->renderer()->xPos() - (m_innerBlock.get() ? m_innerBlock->renderer()->xPos() : 0),
                                      localPoint.y() - m_innerText->renderer()->yPos() - (m_innerBlock.get() ? m_innerBlock->renderer()->yPos() : 0)));
        
        return true;
    }

    return false;
}

IntRect RenderTextControl::controlClipRect(int tx, int ty) const
{
    IntRect clipRect = contentBox();
    clipRect.move(tx, ty);
    return clipRect;
}

void RenderTextControl::layout()
{
    int oldHeight = m_height;
    calcHeight();
    bool relayoutChildren = oldHeight != m_height;

    // Set the text block's height
    int textBlockHeight = m_height - paddingTop() - paddingBottom() - borderTop() - borderBottom();
    int currentTextBlockHeight = m_innerText->renderer()->height();
    if (m_multiLine || m_innerBlock || currentTextBlockHeight > m_height) {
        if (textBlockHeight != currentTextBlockHeight)
            relayoutChildren = true;
        m_innerText->renderer()->style()->setHeight(Length(textBlockHeight, Fixed));
    }
    if (m_innerBlock) {
        if (textBlockHeight != m_innerBlock->renderer()->height())
            relayoutChildren = true;
        m_innerBlock->renderer()->style()->setHeight(Length(textBlockHeight, Fixed));
    }

    int oldWidth = m_width;
    calcWidth();
    if (oldWidth != m_width)
        relayoutChildren = true;

    int searchExtrasWidth = 0;
    if (m_resultsButton && m_resultsButton->renderer()) {
        m_resultsButton->renderer()->calcWidth();
        searchExtrasWidth += m_resultsButton->renderer()->width();
    }
    if (m_cancelButton && m_cancelButton->renderer()) {
        m_cancelButton->renderer()->calcWidth();
        searchExtrasWidth += m_cancelButton->renderer()->width();
    }

    // Set the text block's width
    int textBlockWidth = m_width - paddingLeft() - paddingRight() - borderLeft() - borderRight() -
                         m_innerText->renderer()->paddingLeft() - m_innerText->renderer()->paddingRight() - searchExtrasWidth;
    if (textBlockWidth != m_innerText->renderer()->width())
        relayoutChildren = true;
    m_innerText->renderer()->style()->setWidth(Length(textBlockWidth, Fixed));
    if (m_innerBlock) {
        int innerBlockWidth = m_width - paddingLeft() - paddingRight() - borderLeft() - borderRight();
        if (innerBlockWidth != m_innerBlock->renderer()->width())
            relayoutChildren = true;
        m_innerBlock->renderer()->style()->setWidth(Length(innerBlockWidth, Fixed));
    }

    RenderBlock::layoutBlock(relayoutChildren);
    
    // For text fields, center the inner text vertically
    // Don't do this for search fields, since we don't honor height for them
    if (!m_multiLine) {
        currentTextBlockHeight = m_innerText->renderer()->height();
        if (!m_innerBlock && currentTextBlockHeight < m_height)
            m_innerText->renderer()->setPos(m_innerText->renderer()->xPos(), (m_height - currentTextBlockHeight) / 2);
    }
}

void RenderTextControl::paint(PaintInfo& paintInfo, int tx, int ty)
{
    RenderBlock::paint(paintInfo, tx, ty);
    if (paintInfo.phase == PaintPhaseBlockBackground && m_shouldDrawCapsLockIndicator) {
        IntRect contentsRect = contentBox();
        // Convert the rect into the coords used for painting the content
        contentsRect.move(tx + xPos(), ty + yPos());
        theme()->paintCapsLockIndicator(this, paintInfo, contentsRect);
    }
} 

void RenderTextControl::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Figure out how big a text control needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar ch = '0';
        float charWidth = style()->font().floatWidth(TextRun(&ch, 1, false, 0, 0, false, false, false));
        int factor;
        int scrollbarSize = 0;
        if (m_multiLine) {
            factor = static_cast<HTMLTextAreaElement*>(node())->cols();
            // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
            if (m_innerText->renderer()->style()->overflowY() != OHIDDEN)
                scrollbarSize = ScrollbarTheme::nativeTheme()->scrollbarThickness();
        } else {
            factor = static_cast<HTMLInputElement*>(node())->size();
            if (factor <= 0)
                factor = 20;
        }
        m_maxPrefWidth = static_cast<int>(ceilf(charWidth * factor)) + scrollbarSize +
                         m_innerText->renderer()->paddingLeft() + m_innerText->renderer()->paddingRight();
                
        if (m_resultsButton && m_resultsButton->renderer())
            m_maxPrefWidth += m_resultsButton->renderer()->borderLeft() + m_resultsButton->renderer()->borderRight() +
                              m_resultsButton->renderer()->paddingLeft() + m_resultsButton->renderer()->paddingRight();
        if (m_cancelButton && m_cancelButton->renderer())
            m_maxPrefWidth += m_cancelButton->renderer()->borderLeft() + m_cancelButton->renderer()->borderRight() +
                              m_cancelButton->renderer()->paddingLeft() + m_cancelButton->renderer()->paddingRight();
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

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();

    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false);
}

void RenderTextControl::forwardEvent(Event* evt)
{
    if (evt->type() == eventNames().blurEvent) {
        RenderObject* innerRenderer = m_innerText->renderer();
        if (innerRenderer) {
            RenderLayer* innerLayer = innerRenderer->layer();
            if (innerLayer && !m_multiLine)
                innerLayer->scrollToOffset(style()->direction() == RTL ? innerLayer->scrollWidth() : 0, 0);
        }
        capsLockStateMayHaveChanged();
    } else if (evt->type() == eventNames().focusEvent)
        capsLockStateMayHaveChanged();
    else {
        FloatPoint localPoint = m_innerText->renderer()->absoluteToLocal(FloatPoint(static_cast<MouseEvent*>(evt)->pageX(), static_cast<MouseEvent*>(evt)->pageY()), false, true);
        if (evt->isMouseEvent() && m_resultsButton && localPoint.x() < m_innerText->renderer()->borderBox().x())
            m_resultsButton->defaultEventHandler(evt);
        else if (evt->isMouseEvent() && m_cancelButton && localPoint.x() > m_innerText->renderer()->borderBox().right())
            m_cancelButton->defaultEventHandler(evt);
        else
            m_innerText->defaultEventHandler(evt);
    }
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    HTMLFormControlElement* element = static_cast<HTMLFormControlElement*>(node());
    if (m_multiLine)
        static_cast<HTMLTextAreaElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    else
        static_cast<HTMLInputElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    if (Frame* frame = document()->frame())
        if (frame->selection()->isRange() && userTriggered)
            element->dispatchEventForType(eventNames().selectEvent, true, false);
}

void RenderTextControl::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    graphicsContext->addFocusRingRect(IntRect(tx, ty, width(), height()));
}

void RenderTextControl::autoscroll()
{
    RenderLayer* layer = m_innerText->renderer()->layer();
    if (layer)
        layer->autoscroll();
}

int RenderTextControl::scrollWidth() const
{
    if (m_innerText)
        return m_innerText->scrollWidth();
    return RenderBlock::scrollWidth();
}

int RenderTextControl::scrollHeight() const
{
    if (m_innerText)
        return m_innerText->scrollHeight();
    return RenderBlock::scrollHeight();
}

int RenderTextControl::scrollLeft() const
{
    if (m_innerText)
        return m_innerText->scrollLeft();
    return RenderBlock::scrollLeft();
}

int RenderTextControl::scrollTop() const
{
    if (m_innerText)
        return m_innerText->scrollTop();
    return RenderBlock::scrollTop();
}

void RenderTextControl::setScrollLeft(int newLeft)
{
    if (m_innerText)
        m_innerText->setScrollLeft(newLeft);
}

void RenderTextControl::setScrollTop(int newTop)
{
    if (m_innerText)
        m_innerText->setScrollTop(newTop);
}

const AtomicString& RenderTextControl::autosaveName() const
{
    return static_cast<Element*>(node())->getAttribute(autosaveAttr);
}

void RenderTextControl::addSearchResult()
{
    ASSERT(!m_multiLine);

    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (input->maxResults() <= 0)
        return;

    String value = input->value();
    if (value.isEmpty())
        return;

    Settings* settings = document()->settings();
    if (!settings || settings->privateBrowsingEnabled())
        return;

    int size = static_cast<int>(m_recentSearches.size());
    for (int i = size - 1; i >= 0; --i)
        if (m_recentSearches[i] == value)
            m_recentSearches.remove(i);

    m_recentSearches.insert(0, value);
    while (static_cast<int>(m_recentSearches.size()) > input->maxResults())
        m_recentSearches.removeLast();

    const AtomicString& name = autosaveName();
    if (!m_searchPopup)
        m_searchPopup = SearchPopupMenu::create(this);
    m_searchPopup->saveRecentSearches(name, m_recentSearches);
}

void RenderTextControl::showPopup()
{
    if (m_searchPopupIsVisible)
        return;

    if (!m_searchPopup)
        m_searchPopup = SearchPopupMenu::create(this);

    if (!m_searchPopup->enabled())
        return;
    
    m_searchPopupIsVisible = true;

    const AtomicString& name = autosaveName();
    m_searchPopup->loadRecentSearches(name, m_recentSearches);

    // Trim the recent searches list if the maximum size has changed since we last saved.
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (static_cast<int>(m_recentSearches.size()) > input->maxResults()) {
        do
            m_recentSearches.removeLast();
        while (static_cast<int>(m_recentSearches.size()) > input->maxResults());
        m_searchPopup->saveRecentSearches(name, m_recentSearches);
    }

    m_searchPopup->show(absoluteBoundingBoxRect(true), document()->view(), -1);
}

void RenderTextControl::hidePopup()
{
    if (m_searchPopup)
        m_searchPopup->hide();
    m_searchPopupIsVisible = false;
}

void RenderTextControl::valueChanged(unsigned listIndex, bool fireEvents)
{
    ASSERT(static_cast<int>(listIndex) < listSize());
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (static_cast<int>(listIndex) == (listSize() - 1)) {
        if (fireEvents) {
            m_recentSearches.clear();
            const AtomicString& name = autosaveName();
            if (!name.isEmpty()) {
                if (!m_searchPopup)
                    m_searchPopup = SearchPopupMenu::create(this);
                m_searchPopup->saveRecentSearches(name, m_recentSearches);
            }
        }
    } else {
        input->setValue(itemText(listIndex));
        if (fireEvents)
            input->onSearch();
        input->select();
    }
}

String RenderTextControl::itemText(unsigned listIndex) const
{
    int size = listSize();
    if (size == 1) {
        ASSERT(!listIndex);
        return searchMenuNoRecentSearchesText();
    }
    if (!listIndex)
        return searchMenuRecentSearchesText();
    if (itemIsSeparator(listIndex))
        return String();
    if (static_cast<int>(listIndex) == (size - 1))
        return searchMenuClearRecentSearchesText();
    return m_recentSearches[listIndex - 1];
}

bool RenderTextControl::itemIsEnabled(unsigned listIndex) const
{
     if (!listIndex || itemIsSeparator(listIndex))
        return false;
    return true;
}

PopupMenuStyle RenderTextControl::itemStyle(unsigned listIndex) const
{
    return menuStyle();
}

PopupMenuStyle RenderTextControl::menuStyle() const
{
    return PopupMenuStyle(style()->color(), style()->backgroundColor(), style()->font(), style()->visibility() == VISIBLE);
}

HostWindow* RenderTextControl::hostWindow() const
{
    return document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> RenderTextControl::createScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    RefPtr<Scrollbar> widget;
    bool hasCustomScrollbarStyle = style()->hasPseudoStyle(RenderStyle::SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(client, orientation, this);
    else
        widget = Scrollbar::createNativeScrollbar(client, orientation, controlSize);
    return widget.release();
}

int RenderTextControl::clientInsetLeft() const
{
    // Inset the menu by the radius of the cap on the left so that
    // it only runs along the straight part of the bezel.
    return height() / 2;
}

int RenderTextControl::clientInsetRight() const
{
    // Inset the menu by the radius of the cap on the right so that
    // it only runs along the straight part of the bezel (unless it needs
    // to be wider).
    return height() / 2;
}

int RenderTextControl::clientPaddingLeft() const
{
    int padding = paddingLeft();
    if (m_resultsButton->renderer())
        padding += m_resultsButton->renderer()->width(); 
    return padding;
}

int RenderTextControl::clientPaddingRight() const
{
    int padding = paddingRight();
    if (m_cancelButton->renderer())
        padding += m_cancelButton->renderer()->width(); 
    return padding;
}

int RenderTextControl::listSize() const
{
    // If there are no recent searches, then our menu will have 1 "No recent searches" item.
    if (!m_recentSearches.size())
        return 1;
    // Otherwise, leave room in the menu for a header, a separator, and the "Clear recent searches" item.
    return m_recentSearches.size() + 3;
}

int RenderTextControl::selectedIndex() const
{
    return -1;
}

bool RenderTextControl::itemIsSeparator(unsigned listIndex) const
{
   // The separator will be the second to last item in our list.
   return static_cast<int>(listIndex) == (listSize() - 2);
}

bool RenderTextControl::itemIsLabel(unsigned listIndex) const
{
    return listIndex == 0;
}

bool RenderTextControl::itemIsSelected(unsigned listIndex) const
{
    return false;
}

void RenderTextControl::setTextFromItem(unsigned listIndex)
{
    static_cast<HTMLInputElement*>(node())->setValue(itemText(listIndex));
}

bool RenderTextControl::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    RenderLayer* layer = m_innerText->renderer()->layer();
    if (layer && layer->scroll(direction, granularity, multiplier))
        return true;
    return RenderObject::scroll(direction, granularity, multiplier);
}

void RenderTextControl::searchEventTimerFired(Timer<RenderTextControl>*)
{
    static_cast<HTMLInputElement*>(node())->onSearch();
}

void RenderTextControl::stopSearchEventTimer()
{
    m_searchEventTimer.stop();
}

void RenderTextControl::startSearchEventTimer()
{
    unsigned length = text().length();

    // If there's no text, fire the event right away.
    if (!length) {
        m_searchEventTimer.stop();
        static_cast<HTMLInputElement*>(node())->onSearch();
        return;
    }

    // After typing the first key, we wait 0.5 seconds.
    // After the second key, 0.4 seconds, then 0.3, then 0.2 from then on.
    m_searchEventTimer.startOneShot(max(0.2, 0.6 - 0.1 * length));
}

bool RenderTextControl::isScrollable() const
{
    if (m_innerText && m_innerText->renderer()->isScrollable())
        return true;
    return RenderObject::isScrollable();
}

FontSelector* RenderTextControl::fontSelector() const
{
    return document()->styleSelector()->fontSelector();
}

void RenderTextControl::updatePlaceholderVisibility()
{
    RenderStyle* parentStyle = m_innerBlock ? m_innerBlock->renderer()->style() : style();
    RefPtr<RenderStyle> textBlockStyle = createInnerTextStyle(parentStyle);
    m_innerText->renderer()->setStyle(textBlockStyle);
    for (Node* n = m_innerText->firstChild(); n; n = n->traverseNextNode(m_innerText.get())) {
        if (n->renderer())
            n->renderer()->setStyle(textBlockStyle);
    }
    updateFromElement();
}

void RenderTextControl::capsLockStateMayHaveChanged()
{
    // Only draw the caps lock indicator if these things are true:
    // 1) The field is a password field
    // 2) The frame is active
    // 3) The element is focused
    // 4) The caps lock is on

    bool shouldDrawCapsLockIndicator = false;
    if (Node* n = node())
        if (Document* d = document())
            if (Frame* f = d->frame())
                shouldDrawCapsLockIndicator = !m_multiLine && static_cast<HTMLInputElement*>(n)->inputType() == HTMLInputElement::PASSWORD && 
                                               f->selection()->isFocusedAndActive() && d->focusedNode() == n && PlatformKeyboardEvent::currentCapsLockState();

    if (shouldDrawCapsLockIndicator != m_shouldDrawCapsLockIndicator) {
        m_shouldDrawCapsLockIndicator = shouldDrawCapsLockIndicator;
        repaint();
    }
}

} // namespace WebCore
