/**
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "RenderTheme.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "TextStyle.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <math.h>

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;
using namespace std;

RenderTextControl::RenderTextControl(Node* node, bool multiLine)
    : RenderBlock(node), m_dirty(false), m_multiLine(multiLine)
{
}

RenderTextControl::~RenderTextControl()
{
    if (m_multiLine && node())
        static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
    // The renderer for the div has already been destroyed by destroyLeftoverChildren
    if (m_div)
        m_div->detach();
}

void RenderTextControl::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_div) {
        RenderBlock* divRenderer = static_cast<RenderBlock*>(m_div->renderer());
        RenderStyle* divStyle = createDivStyle(style);
        divRenderer->setStyle(divStyle);
        for (Node *n = m_div->firstChild(); n; n = n->traverseNextNode(m_div.get()))
            if (n->renderer())
                n->renderer()->setStyle(divStyle);
    }
    setHasOverflowClip(false);
    setReplaced(isInline());
}

RenderStyle* RenderTextControl::createDivStyle(RenderStyle* startStyle)
{
    RenderStyle* divStyle = new (renderArena()) RenderStyle();
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    
    divStyle->inheritFrom(startStyle);
    divStyle->setDisplay(BLOCK);
    divStyle->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);

    if (m_multiLine) {
        // Forward overflow properties.
        divStyle->setOverflowX(startStyle->overflowX() == OVISIBLE ? OAUTO : startStyle->overflowX());
        divStyle->setOverflowY(startStyle->overflowY() == OVISIBLE ? OAUTO : startStyle->overflowY());

        // Set word wrap property based on wrap attribute
        if (static_cast<HTMLTextAreaElement*>(element)->wrap() == HTMLTextAreaElement::ta_NoWrap) {
            divStyle->setWhiteSpace(PRE);
            divStyle->setWordWrap(WBNORMAL);
        } else {
            divStyle->setWhiteSpace(PRE_WRAP);
            divStyle->setWordWrap(BREAK_WORD);
        }

    } else {
        divStyle->setWhiteSpace(PRE);
        divStyle->setOverflowX(OHIDDEN);
        divStyle->setOverflowY(OHIDDEN);
    }

    if (!m_multiLine) {
        // We're adding one extra pixel of padding to match WinIE.
        divStyle->setPaddingLeft(Length(1, Fixed));
        divStyle->setPaddingRight(Length(1, Fixed));
    } else {
        // We're adding three extra pixels of padding to line textareas up with text fields.
        divStyle->setPaddingLeft(Length(3, Fixed));
        divStyle->setPaddingRight(Length(3, Fixed));
    }

    if (!element->isEnabled()) {
        Color textColor = startStyle->color();
        Color disabledTextColor;
        if (differenceSquared(textColor, Color::white) > differenceSquared(startStyle->backgroundColor(), Color::white))
            disabledTextColor = textColor.light();
        else
            disabledTextColor = textColor.dark();
        divStyle->setColor(disabledTextColor);
    }

    return divStyle;
}

void RenderTextControl::updateFromElement()
{
    if (!m_div) {
        // Create the div and give it a parent, renderer, and style
        m_div = new HTMLTextFieldInnerElement(document(), node());
        RenderBlock* divRenderer = new (renderArena()) RenderBlock(m_div.get());
        m_div->setRenderer(divRenderer);
        m_div->setAttached();
        m_div->setInDocument(true);
        
        RenderStyle* divStyle = createDivStyle(style());
        divRenderer->setStyle(divStyle); 
        
        // Add div to Render tree
        RenderBlock::addChild(divRenderer);
    }

    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    m_div->renderer()->style()->setUserModify(element->isReadOnlyControl() || element->disabled() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
    String value;
    if (m_multiLine)
        value = static_cast<HTMLTextAreaElement*>(element)->value().copy();    
    else
        value = static_cast<HTMLInputElement*>(element)->value().copy();        
    if (!element->valueMatchesRenderer() || m_multiLine) {
        String oldText = text();
        if (value.isNull())
            value = "";
        value.replace('\\', backslashAsCurrencySymbol());
        if (value != oldText || !m_div->hasChildNodes()) {
            ExceptionCode ec = 0;
            m_div->setInnerText(value, ec);
            if (value.endsWith("\n") || value.endsWith("\r"))
                m_div->appendChild(new HTMLBRElement(document()), ec);
            if (document()->frame())
                document()->frame()->editor()->clearUndoRedoOperations();
            setEdited(false);
        }
        element->setValueMatchesRenderer();
    }
}

int RenderTextControl::selectionStart()
{
    return indexForVisiblePosition(document()->frame()->selectionController()->start());        
}

int RenderTextControl::selectionEnd()
{
    return indexForVisiblePosition(document()->frame()->selectionController()->end());
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
    document()->frame()->selectionController()->setSelection(newSelection);
    // FIXME: Granularity is stored separately on the frame, but also in the selection controller.
    // The granularity in the selection controller should be used, and then this line of code would not be needed.
    document()->frame()->setSelectionGranularity(CharacterGranularity);
}

VisiblePosition RenderTextControl::visiblePositionForIndex(int index)
{    
    if (index <= 0)
        return VisiblePosition(m_div.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_div.get(), ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(it.range()->endContainer(ec), it.range()->endOffset(ec), UPSTREAM);
}

int RenderTextControl::indexForVisiblePosition(const VisiblePosition& pos)
{
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != m_div)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->setStart(m_div.get(), 0, ec);
    range->setEnd(indexPosition.node(), indexPosition.offset(), ec);
    return TextIterator::rangeLength(range.get());
}

void RenderTextControl::subtreeHasChanged()
{
    bool wasPreviouslyEdited = isEdited();
    setEdited(true);
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine) {
        element->setValueMatchesRenderer(false);
        document()->frame()->textDidChangeInTextArea(element);
    } else {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        if (input) {
            input->setValueFromRenderer(text());
            if (!wasPreviouslyEdited)
                document()->frame()->textFieldDidBeginEditing(input);
            document()->frame()->textDidChangeInTextField(input);
        }
    }
}

String RenderTextControl::text()
{
    if (m_div)
        return m_div->textContent().replace('\\', backslashAsCurrencySymbol());
    return String();
}

String RenderTextControl::textWithHardLineBreaks()
{
    String s("");
    
    if (!m_div || !m_div->firstChild())
        return s;

    document()->updateLayout();

    RenderObject* renderer = m_div->firstChild()->renderer();
    if (!renderer)
        return s;

    InlineBox* box = renderer->inlineBox(0, DOWNSTREAM);
    if (!box)
        return s;

    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_div.get(), ec);
    for (RootInlineBox* line = box->root(); line; line = line->nextRootBox()) {
        // If we're at a soft wrap, then insert the hard line break here
        if (!line->endsWithBreak() && line->nextRootBox()) {
            // Update range so it ends before this wrap
            ASSERT(line->lineBreakObj());
            range->setEnd(line->lineBreakObj()->node(), line->lineBreakPos(), ec);

            s.append(range->toString(true, ec));
            s.append("\n");

            // Update range so it starts after this wrap
            range->setEnd(m_div.get(), maxDeepOffset(m_div.get()), ec);
            range->setStart(line->lineBreakObj()->node(), line->lineBreakPos(), ec);
        }
    }
    s.append(range->toString(true, ec));
    ASSERT(ec == 0);

    return s.replace('\\', backslashAsCurrencySymbol());
}

void RenderTextControl::calcHeight()
{
    
    int rows = 1;
    if (m_multiLine)
        rows = static_cast<HTMLTextAreaElement*>(node())->rows();
    
    int line = m_div->renderer()->lineHeight(true);
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom() +
                m_div->renderer()->paddingTop() + m_div->renderer()->paddingBottom();
    
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
    int scrollbarSize = 0;
    // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
    if (m_div->renderer()->style()->overflowX() == OSCROLL ||  (m_div->renderer()->style()->overflowX() == OAUTO && m_div->renderer()->style()->wordWrap() == WBNORMAL))
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
    // If we're within the text control, we want to act as if we've hit the inner div, incase the point 
    // was on the control but not on the div (see Radar 4617841).
    if (RenderBlock::nodeAtPoint(request, result, x, y, tx, ty, hitTestAction)) {
        result.setInnerNode(m_div.get());
        return true;
    }  
    return false;
}

void RenderTextControl::layout()
{    
    int oldHeight = m_height;
    calcHeight();
    bool relayoutChildren = oldHeight != m_height;
    
    // Set the div's height
    int divHeight = m_height - paddingTop() - paddingBottom() - borderTop() - borderBottom();
    m_div->renderer()->style()->setHeight(Length(divHeight, Fixed));

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
            if (m_div->renderer()->style()->overflowY() != OHIDDEN)
                scrollbarSize = 15;
        }
        else {
            factor = static_cast<HTMLInputElement*>(node())->size();
            if (factor <= 0)
                factor = 20;
        }
        m_maxWidth = (int)ceilf(charWidth * factor) + scrollbarSize;
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
                m_div->renderer()->paddingLeft() + m_div->renderer()->paddingRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();    
}

void RenderTextControl::forwardEvent(Event* evt)
{
    if (evt->type() == blurEvent) {
        RenderObject* innerRenderer = m_div->renderer();
        if (innerRenderer) {
            RenderLayer* innerLayer = innerRenderer->layer();
            if (innerLayer && !m_multiLine)
                innerLayer->scrollToOffset(style()->direction() == RTL ? innerLayer->scrollWidth() : 0, 0);
        }
    } else
        m_div->defaultEventHandler(evt);
}

void RenderTextControl::selectionChanged(bool userTriggered)
{
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine)
        static_cast<HTMLTextAreaElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    else
        static_cast<HTMLInputElement*>(element)->cacheSelection(selectionStart(), selectionEnd());
    if (document()->frame()->selectionController()->isRange() && userTriggered)
        element->onSelect();
}

int RenderTextControl::scrollWidth() const
{
    if (m_div)
        return m_div->scrollWidth();
    return RenderBlock::scrollWidth();
}

int RenderTextControl::scrollHeight() const
{
    if (m_div)
        return m_div->scrollHeight();
    return RenderBlock::scrollHeight();
}

int RenderTextControl::scrollLeft() const
{
    if (m_div)
        return m_div->scrollLeft();
    return RenderBlock::scrollLeft();
}

int RenderTextControl::scrollTop() const
{
    if (m_div)
        return m_div->scrollTop();
    return RenderBlock::scrollTop();
}

void RenderTextControl::setScrollLeft(int newLeft)
{
    if (m_div)
        m_div->setScrollLeft(newLeft);
}

void RenderTextControl::setScrollTop(int newTop)
{
    if (m_div)
        m_div->setScrollTop(newTop);
}

}
