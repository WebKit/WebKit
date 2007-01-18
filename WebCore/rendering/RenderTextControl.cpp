/**
 * Copyright (C) 2006, 2007 Apple Inc.
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

#include "config.h"
#include "RenderTextControl.h"

#include "Document.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFieldInnerElement.h"
#include "HitTestResult.h"
#include "MouseEvent.h"
#include "RenderTheme.h"
#include "SearchPopupMenu.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TextIterator.h"
#include "TextStyle.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

RenderTextControl::RenderTextControl(Node* node, bool multiLine)
    : RenderBlock(node)
    , m_dirty(false)
    , m_multiLine(multiLine)
    , m_placeholderVisible(false)
    , m_searchPopup(0)
    , m_searchPopupIsVisible(false)
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

void RenderTextControl::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_innerBlock)
        m_innerBlock->renderer()->setStyle(createInnerBlockStyle(style));

    if (m_innerText) {
        RenderBlock* textBlockRenderer = static_cast<RenderBlock*>(m_innerText->renderer());
        RenderStyle* textBlockStyle = createInnerTextStyle(style);
        textBlockRenderer->setStyle(textBlockStyle);
        for (Node* n = m_innerText->firstChild(); n; n = n->traverseNextNode(m_innerText.get())) {
            if (n->renderer())
                n->renderer()->setStyle(textBlockStyle);
        }
    }
    if (m_resultsButton)
        m_resultsButton->renderer()->setStyle(createResultsButtonStyle(style));

    if (m_cancelButton)
        m_cancelButton->renderer()->setStyle(createCancelButtonStyle(style));

    setHasOverflowClip(false);
    setReplaced(isInline());
}

static Color disabledTextColor(const Color& textColor, const Color& backgroundColor)
{
    if (differenceSquared(textColor, Color::white) > differenceSquared(backgroundColor, Color::white))
        return textColor.light();
    return textColor.dark();
}

RenderStyle* RenderTextControl::createInnerBlockStyle(RenderStyle* startStyle)
{
    RenderStyle* innerBlockStyle = new (renderArena()) RenderStyle();

    innerBlockStyle->inheritFrom(startStyle);
    innerBlockStyle->setDisplay(BLOCK);
    // We don't want the shadow dom to be editable, so we set this block to read-only in case the input itself is editable.
    innerBlockStyle->setUserModify(READ_ONLY);

    return innerBlockStyle;
}

RenderStyle* RenderTextControl::createInnerTextStyle(RenderStyle* startStyle)
{
    RenderStyle* textBlockStyle = new (renderArena()) RenderStyle();
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());

    textBlockStyle->inheritFrom(startStyle);
    textBlockStyle->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    if (m_innerBlock)
        textBlockStyle->setDisplay(INLINE_BLOCK);
    else
        textBlockStyle->setDisplay(BLOCK);

    if (m_multiLine) {
        // Forward overflow properties.
        textBlockStyle->setOverflowX(startStyle->overflowX() == OVISIBLE ? OAUTO : startStyle->overflowX());
        textBlockStyle->setOverflowY(startStyle->overflowY() == OVISIBLE ? OAUTO : startStyle->overflowY());

        // Set word wrap property based on wrap attribute
        if (static_cast<HTMLTextAreaElement*>(element)->wrap() == HTMLTextAreaElement::ta_NoWrap) {
            textBlockStyle->setWhiteSpace(PRE);
            textBlockStyle->setWordWrap(WBNORMAL);
        } else {
            textBlockStyle->setWhiteSpace(PRE_WRAP);
            textBlockStyle->setWordWrap(BREAK_WORD);
        }
    } else {
        textBlockStyle->setWhiteSpace(PRE);
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

    if (!element->isEnabled())
        textBlockStyle->setColor(disabledTextColor(startStyle->color(), startStyle->backgroundColor()));

    return textBlockStyle;
}

RenderStyle* RenderTextControl::createResultsButtonStyle(RenderStyle* startStyle)
{
    ASSERT(!m_multiLine);
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    RenderStyle* resultsBlockStyle;
    if (input->maxResults() < 0)
        resultsBlockStyle = getPseudoStyle(RenderStyle::SEARCH_DECORATION);
    else if (!input->maxResults())
        resultsBlockStyle = getPseudoStyle(RenderStyle::SEARCH_RESULTS_DECORATION);
    else
        resultsBlockStyle = getPseudoStyle(RenderStyle::SEARCH_RESULTS_BUTTON);

    if (!resultsBlockStyle)
        resultsBlockStyle = new (renderArena()) RenderStyle();

    if (startStyle)
        resultsBlockStyle->inheritFrom(startStyle);

    resultsBlockStyle->setDisplay(INLINE_BLOCK);

    return resultsBlockStyle;
}

RenderStyle* RenderTextControl::createCancelButtonStyle(RenderStyle* startStyle)
{
    RenderStyle* cancelBlockStyle = getPseudoStyle(RenderStyle::SEARCH_CANCEL_BUTTON);
    if (!cancelBlockStyle)
        cancelBlockStyle = new (renderArena()) RenderStyle();

    if (startStyle)
        cancelBlockStyle->inheritFrom(startStyle);

    cancelBlockStyle->setDisplay(INLINE_BLOCK);

    updateCancelButtonVisibility(cancelBlockStyle);

    return cancelBlockStyle;
}

void RenderTextControl::updatePlaceholder()
{
    String placeholder;
    if (!m_multiLine) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
        if (input->value().isEmpty() && document()->focusedNode() != node())
            placeholder = input->getAttribute(placeholderAttr);
    }

    if (!placeholder.isEmpty() || m_placeholderVisible) {
        ExceptionCode ec = 0;
        m_innerText->setInnerText(placeholder, ec);
        m_placeholderVisible = !placeholder.isEmpty();
    }

    Color color;
    if (!placeholder.isEmpty())
        color = Color::darkGray;
    else if (node()->isEnabled())
        color = style()->color();
    else
        color = disabledTextColor(style()->color(), style()->backgroundColor());

    RenderObject* renderer = m_innerText->renderer();
    RenderStyle* style = renderer->style();
    if (style->color() != color) {
        style->setColor(color);
        renderer->repaint();
    }
}

void RenderTextControl::createSubtreeIfNeeded()
{
    // When adding these elements, create the renderer & style first before adding to the DOM.
    // Otherwise, the render tree will create some anonymous blocks that will mess up our layout.
    bool isSearchField = !m_multiLine && static_cast<HTMLInputElement*>(node())->isSearchField();
    if (isSearchField && !m_innerBlock) {
        // Create the inner block element and give it a parent, renderer, and style
        m_innerBlock = new HTMLTextFieldInnerElement(document(), node());
        RenderBlock* innerBlockRenderer = new (renderArena()) RenderBlock(m_innerBlock.get());
        m_innerBlock->setRenderer(innerBlockRenderer);
        m_innerBlock->setAttached();
        m_innerBlock->setInDocument(true);
        innerBlockRenderer->setStyle(createInnerBlockStyle(style()));

        // Add inner block renderer to Render tree
        RenderBlock::addChild(innerBlockRenderer);
    }
    if (isSearchField && !m_resultsButton) {
        // Create the results block element and give it a parent, renderer, and style
        m_resultsButton = new HTMLSearchFieldResultsButtonElement(document());
        RenderBlock* resultsBlockRenderer = new (renderArena()) RenderBlock(m_resultsButton.get());
        m_resultsButton->setRenderer(resultsBlockRenderer);
        m_resultsButton->setAttached();
        m_resultsButton->setInDocument(true);

        RenderStyle* resultsBlockStyle = createResultsButtonStyle(m_innerBlock->renderer()->style());
        resultsBlockRenderer->setStyle(resultsBlockStyle);

        // Add results renderer to DOM & Render tree
        m_innerBlock->renderer()->addChild(resultsBlockRenderer);
        ExceptionCode ec = 0;
        m_innerBlock->appendChild(m_resultsButton, ec);
    }
    if (!m_innerText) {
        // Create the text block element and give it a parent, renderer, and style
        // For non-search fields, there is no intermediate m_innerBlock as the shadow node.
        // m_innerText will be the shadow node in that case.
        m_innerText = new HTMLTextFieldInnerTextElement(document(), m_innerBlock ? 0 : node());
        RenderBlock* textBlockRenderer = new (renderArena()) RenderBlock(m_innerText.get());
        m_innerText->setRenderer(textBlockRenderer);
        m_innerText->setAttached();
        m_innerText->setInDocument(true);

        RenderStyle* parentStyle = style();
        if (m_innerBlock)
            parentStyle = m_innerBlock->renderer()->style();
        RenderStyle* textBlockStyle = createInnerTextStyle(parentStyle);
        textBlockRenderer->setStyle(textBlockStyle);

        // Add text block renderer to Render tree
        if (m_innerBlock) {
            m_innerBlock->renderer()->addChild(textBlockRenderer);
            ExceptionCode ec = 0;
            // Add text block to the DOM
            m_innerBlock->appendChild(m_innerText, ec);
        } else
            RenderBlock::addChild(textBlockRenderer);
    }
    if (isSearchField && !m_cancelButton) {
        // Create the close block element and give it a parent, renderer, and style
        m_cancelButton = new HTMLSearchFieldCancelButtonElement(document());
        RenderBlock* closeBlockRenderer = new (renderArena()) RenderBlock(m_cancelButton.get());
        m_cancelButton->setRenderer(closeBlockRenderer);
        m_cancelButton->setAttached();
        m_cancelButton->setInDocument(true);

        RenderStyle* closeBlockStyle = createCancelButtonStyle(m_innerBlock->renderer()->style());
        closeBlockRenderer->setStyle(closeBlockStyle);

        // Add close block renderer to DOM & Render tree
        m_innerBlock->renderer()->addChild(closeBlockRenderer);
        ExceptionCode ec = 0;
        m_innerBlock->appendChild(m_cancelButton, ec);
    }
}

void RenderTextControl::updateFromElement()
{
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());

    createSubtreeIfNeeded();

    if (m_cancelButton)
        updateCancelButtonVisibility(m_cancelButton->renderer()->style());

    updatePlaceholder();

    m_innerText->renderer()->style()->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);

    if ((!element->valueMatchesRenderer() || m_multiLine) && !m_placeholderVisible) {
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
            ExceptionCode ec = 0;
            m_innerText->setInnerText(value, ec);
            if (value.endsWith("\n") || value.endsWith("\r"))
                m_innerText->appendChild(new HTMLBRElement(document()), ec);
            if (Frame* frame = document()->frame())
                frame->editor()->clearUndoRedoOperations();
            m_dirty = false;
        }
        element->setValueMatchesRenderer();
    }

    if (m_searchPopupIsVisible)
        m_searchPopup->updateFromElement();
}

int RenderTextControl::selectionStart()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selectionController()->start());
}

int RenderTextControl::selectionEnd()
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;
    return indexForVisiblePosition(frame->selectionController()->end());
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

    if (style()->visibility() == HIDDEN) {
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
        frame->selectionController()->setSelection(newSelection);

    // FIXME: Granularity is stored separately on the frame, but also in the selection controller.
    // The granularity in the selection controller should be used, and then this line of code would not be needed.
    if (Frame* frame = document()->frame())
        frame->setSelectionGranularity(CharacterGranularity);
}

VisiblePosition RenderTextControl::visiblePositionForIndex(int index)
{
    if (index <= 0)
        return VisiblePosition(m_innerText.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
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
    RefPtr<Range> range = new Range(document());
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
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine) {
        element->setValueMatchesRenderer(false);
        if (Frame* frame = document()->frame())
            frame->textDidChangeInTextArea(element);
    } else {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        input->setValueFromRenderer(input->constrainValue(text()));
        if (m_cancelButton)
            updateCancelButtonVisibility(m_cancelButton->renderer()->style());

        // If the incremental attribute is set, then dispatch the search event
        if (!input->getAttribute(incrementalAttr).isNull())
            input->onSearch();

        if (!wasDirty) {
            if (Frame* frame = document()->frame())
                frame->textFieldDidBeginEditing(input);
        }
        if (Frame* frame = document()->frame())
            frame->textDidChangeInTextField(input);
    }
}

String RenderTextControl::text()
{
    if (m_innerText)
        return m_innerText->textContent().replace('\\', backslashAsCurrencySymbol());
    return String();
}

String RenderTextControl::textWithHardLineBreaks()
{
    String s("");

    if (!m_innerText || !m_innerText->firstChild())
        return s;

    document()->updateLayout();

    RenderObject* renderer = m_innerText->firstChild()->renderer();
    if (!renderer)
        return s;

    InlineBox* box = renderer->inlineBox(0, DOWNSTREAM);
    if (!box)
        return s;

    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_innerText.get(), ec);
    for (RootInlineBox* line = box->root(); line; line = line->nextRootBox()) {
        // If we're at a soft wrap, then insert the hard line break here
        if (!line->endsWithBreak() && line->nextRootBox()) {
            // Update range so it ends before this wrap
            ASSERT(line->lineBreakObj());
            range->setEnd(line->lineBreakObj()->node(), line->lineBreakPos(), ec);

            s.append(range->toString(true, ec));
            s.append("\n");

            // Update range so it starts after this wrap
            range->setEnd(m_innerText.get(), maxDeepOffset(m_innerText.get()), ec);
            range->setStart(line->lineBreakObj()->node(), line->lineBreakPos(), ec);
        }
    }
    s.append(range->toString(true, ec));
    ASSERT(!ec);

    return s.replace('\\', backslashAsCurrencySymbol());
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

    if (m_resultsButton) {
        static_cast<RenderBlock*>(m_resultsButton->renderer())->calcHeight();
        innerToAdd = max(innerToAdd,
                         m_resultsButton->renderer()->borderTop() + m_resultsButton->renderer()->borderBottom() +
                         m_resultsButton->renderer()->paddingTop() + m_resultsButton->renderer()->paddingBottom() +
                         m_resultsButton->renderer()->marginTop() + m_resultsButton->renderer()->marginBottom());
        line = max(line, m_resultsButton->renderer()->height());
    }
    if (m_cancelButton) {
        static_cast<RenderBlock*>(m_cancelButton->renderer())->calcHeight();
        innerToAdd = max(innerToAdd,
                         m_cancelButton->renderer()->borderTop() + m_cancelButton->renderer()->borderBottom() +
                         m_cancelButton->renderer()->paddingTop() + m_cancelButton->renderer()->paddingBottom() +
                         m_cancelButton->renderer()->marginTop() + m_cancelButton->renderer()->marginBottom());
        line = max(line, m_cancelButton->renderer()->height());
    }
    toAdd += innerToAdd;

    // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
    int scrollbarSize = 0;
    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (m_innerText->renderer()->style()->overflowX() == OSCROLL ||  (m_innerText->renderer()->style()->overflowX() == OAUTO && m_innerText->renderer()->style()->wordWrap() == WBNORMAL))
        scrollbarSize = 15;

    m_height = line * rows + toAdd + scrollbarSize;

    RenderBlock::calcHeight();
}

short RenderTextControl::baselinePosition(bool b, bool isRootLineBox) const
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

    if (RenderBlock::nodeAtPoint(request, result, x, y, tx, ty, hitTestAction)) {
        Node* leftNode;
        Node* rightNode;
        if (style()->direction() == LTR) {
            leftNode = m_resultsButton.get();
            rightNode = m_cancelButton.get();
        } else {
            leftNode = m_cancelButton.get();
            rightNode = m_resultsButton.get();
        }
        if (leftNode && x < m_innerText->renderer()->absoluteBoundingBoxRect().x())
            result.setInnerNode(leftNode);
        else if (rightNode && x > m_innerText->renderer()->absoluteBoundingBoxRect().right())
            result.setInnerNode(rightNode);
        else
            result.setInnerNode(m_innerText.get());
        return true;
    }
    return false;
}

void RenderTextControl::layout()
{
    int oldHeight = m_height;
    calcHeight();
    bool relayoutChildren = oldHeight != m_height;

    // Set the text block's height
    int textBlockHeight = m_height - paddingTop() - paddingBottom() - borderTop() - borderBottom();
    m_innerText->renderer()->style()->setHeight(Length(textBlockHeight, Fixed));
    if (m_innerBlock)
        m_innerBlock->renderer()->style()->setHeight(Length(textBlockHeight, Fixed));

    int oldWidth = m_width;
    calcWidth();
    if (oldWidth != m_width)
        relayoutChildren = true;

    int searchExtrasWidth = 0;
    if (m_resultsButton) {
        m_resultsButton->renderer()->calcWidth();
        searchExtrasWidth += m_resultsButton->renderer()->width();
    }
    if (m_cancelButton) {
        m_cancelButton->renderer()->calcWidth();
        searchExtrasWidth += m_cancelButton->renderer()->width();
    }

    // Set the text block's width
    int textBlockWidth = m_width - paddingLeft() - paddingRight() - borderLeft() - borderRight() -
                         m_innerText->renderer()->paddingLeft() - m_innerText->renderer()->paddingRight() - searchExtrasWidth;
    m_innerText->renderer()->style()->setWidth(Length(textBlockWidth, Fixed));
    if (m_innerBlock)
        m_innerBlock->renderer()->style()->setWidth(Length(m_width - paddingLeft() - paddingRight() - borderLeft() - borderRight(), Fixed));

    RenderBlock::layoutBlock(relayoutChildren);
}

void RenderTextControl::calcMinMaxWidth()
{
    m_minWidth = 0;
    m_maxWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Figure out how big a text control needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar ch = '0';
        float charWidth = style()->font().floatWidth(TextRun(&ch, 1), TextStyle(0, 0, 0, false, false, false));
        int factor;
        int scrollbarSize = 0;
        if (m_multiLine) {
            factor = static_cast<HTMLTextAreaElement*>(node())->cols();
            // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
            if (m_innerText->renderer()->style()->overflowY() != OHIDDEN)
                scrollbarSize = 15;
        } else {
            factor = static_cast<HTMLInputElement*>(node())->size();
            if (factor <= 0)
                factor = 20;
        }
        m_maxWidth = static_cast<int>(ceilf(charWidth * factor)) + scrollbarSize;
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxWidth = max(m_maxWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minWidth = max(m_minWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minWidth = 0;
    else
        m_minWidth = m_maxWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxWidth = min(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minWidth = min(m_minWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight() +
                m_innerText->renderer()->paddingLeft() + m_innerText->renderer()->paddingRight();

    if (m_resultsButton)
        toAdd += m_resultsButton->renderer()->borderLeft() + m_resultsButton->renderer()->borderRight() +
                 m_resultsButton->renderer()->paddingLeft() + m_resultsButton->renderer()->paddingRight();
    if (m_cancelButton)
        toAdd += m_cancelButton->renderer()->borderLeft() + m_cancelButton->renderer()->borderRight() +
                 m_cancelButton->renderer()->paddingLeft() + m_cancelButton->renderer()->paddingRight();

    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();
}

void RenderTextControl::forwardEvent(Event* evt)
{
    if (evt->type() == blurEvent) {
        RenderObject* innerRenderer = m_innerText->renderer();
        if (innerRenderer) {
            RenderLayer* innerLayer = innerRenderer->layer();
            if (innerLayer && !m_multiLine)
                innerLayer->scrollToOffset(style()->direction() == RTL ? innerLayer->scrollWidth() : 0, 0);
        }
        updatePlaceholder();
    } else if (evt->type() == focusEvent)
        updatePlaceholder();
    else {
        EventTargetNode* leftNode;
        EventTargetNode* rightNode;
        if (style()->direction() == LTR) {
            leftNode = m_resultsButton.get();
            rightNode = m_cancelButton.get();
        } else {
            leftNode = m_cancelButton.get();
            rightNode = m_resultsButton.get();
        }
        if (evt->isMouseEvent() && leftNode && static_cast<MouseEvent*>(evt)->x() < m_innerText->renderer()->absoluteBoundingBoxRect().x())
            leftNode->defaultEventHandler(evt);
        else if (evt->isMouseEvent() && rightNode && static_cast<MouseEvent*>(evt)->x() > m_innerText->renderer()->absoluteBoundingBoxRect().right())
            rightNode->defaultEventHandler(evt);
        else
            m_innerText->defaultEventHandler(evt);
    }
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine)
        static_cast<HTMLTextAreaElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    else
        static_cast<HTMLInputElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    if (Frame* frame = document()->frame())
        if (frame->selectionController()->isRange() && userTriggered)
            element->dispatchHTMLEvent(selectEvent, true, false);
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

    Frame* frame = document()->frame();
    if (!frame || frame->settings()->privateBrowsingEnabled())
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

    m_searchPopup->show(absoluteBoundingBoxRect(), document()->view(), -1);
}

void RenderTextControl::hidePopup()
{
    if (m_searchPopup)
        m_searchPopup->hide();
    m_searchPopupIsVisible = false;
}

void RenderTextControl::valueChanged(unsigned listIndex, bool fireEvents)
{
    ASSERT(listIndex < listSize());
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    if (listIndex == (listSize() - 1)) {
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
    unsigned size = listSize();
    if (size == 1) {
        ASSERT(!listIndex);
        return String("No recent searches");
    }
    if (!listIndex)
        return String("Recent Searches");
    if (itemIsSeparator(listIndex))
        return String();
    if (listIndex == (size - 1))
        return String("Clear Recent Searches");
    return m_recentSearches[listIndex - 1];
}

bool RenderTextControl::itemIsEnabled(unsigned listIndex) const
{
     if (!listIndex || itemIsSeparator(listIndex))
        return false;
    return true;
}

RenderStyle* RenderTextControl::itemStyle(unsigned listIndex) const
{
    return style();
}

RenderStyle* RenderTextControl::clientStyle() const
{
    return style();
}

Document* RenderTextControl::clientDocument() const
{
    return document();
}

int RenderTextControl::clientPaddingLeft() const
{
    if (style()->direction() == LTR)
        return paddingLeft() + m_resultsButton->renderer()->width();
    return paddingLeft() + m_cancelButton->renderer()->width();
}

int RenderTextControl::clientPaddingRight() const
{
    if (style()->direction() == LTR)
        return paddingRight() + m_cancelButton->renderer()->width();
    return paddingRight() + m_resultsButton->renderer()->width();
}

unsigned RenderTextControl::listSize() const
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
   return listIndex == (listSize() - 2);
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

} // namespace WebCore
