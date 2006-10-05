/**
 * This file is part of the select element renderer in WebCore.
 *
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
#include "RenderMenuList.h"

#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "RenderPopupMenu.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "TextStyle.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderMenuList::RenderMenuList(HTMLSelectElement* element)
    : RenderFlexibleBox(element)
    , m_buttonText(0)
    , m_innerBlock(0)
    , m_optionsChanged(true)
    , m_optionsWidth(0)
    , m_popup(0)
    , m_popupIsVisible(false)
{
}

RenderMenuList::~RenderMenuList()
{
    if (m_popup)
        m_popup->destroy();
    m_popup = 0;
}

void RenderMenuList::createInnerBlock()
{
    if (m_innerBlock) {
        ASSERT(firstChild() == m_innerBlock);
        ASSERT(!m_innerBlock->nextSibling());
        return;
    }

    // Create an anonymous block.
    ASSERT(!firstChild());
    m_innerBlock = createAnonymousBlock();
    m_innerBlock->style()->setBoxFlex(1.0f);
    RenderFlexibleBox::addChild(m_innerBlock);
}

void RenderMenuList::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    createInnerBlock();
    m_innerBlock->addChild(newChild, beforeChild);
}

void RenderMenuList::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_innerBlock || !m_innerBlock) {
        RenderFlexibleBox::removeChild(oldChild);
        m_innerBlock = 0;
    } else
        m_innerBlock->removeChild(oldChild);
}

void RenderMenuList::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_buttonText)
        m_buttonText->setStyle(style);
    if (m_innerBlock)
        m_innerBlock->style()->setBoxFlex(1.0f);
    setReplaced(isInline());
}

void RenderMenuList::updateFromElement()
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    int size = listItems.size();

    if (m_optionsChanged) {        
        float width = 0;
        TextStyle textStyle(0, 0, 0, false, false, false, false);
        for (int i = 0; i < size; ++i) {
            HTMLElement* element = listItems[i];
            if (element->hasTagName(optionTag)) {
                String text = static_cast<HTMLOptionElement*>(element)->optionText();
                if (!text.isEmpty())
                    width = max(width, style()->font().floatWidth(TextRun(text.impl()), textStyle));
            }
        }
        m_optionsWidth = static_cast<int>(ceilf(width));
        m_optionsChanged = false;
    }

    int i = select->optionToListIndex(select->selectedIndex());
    String text = "";
    if (i >= 0 && i < size) {
        HTMLElement* element = listItems[i];
        if (element->hasTagName(optionTag))
            text = static_cast<HTMLOptionElement*>(listItems[i])->optionText();
    }
    setText(text);
}

void RenderMenuList::setText(const String& s)
{
    if (s.isEmpty()) {
        if (m_buttonText) {
            m_buttonText->destroy();
            m_buttonText = 0;
        }
    } else {
        if (m_buttonText)
            m_buttonText->setText(s.impl());
        else {
            m_buttonText = new (renderArena()) RenderText(document(), s.impl());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
    }
}


String RenderMenuList::text()
{
    return m_buttonText ? m_buttonText->data() : String();
}

void RenderMenuList::paintObject(PaintInfo& i, int x, int y)
{
    // Push a clip.
    if (i.phase == PaintPhaseForeground) {
        IntRect clipRect(x + borderLeft() + paddingLeft(), y + borderTop() + paddingTop(),
            width() - borderLeft() - borderRight() - paddingLeft() - paddingRight(),
            height() - borderBottom() - borderTop() - paddingTop() - paddingBottom());
        if (clipRect.isEmpty())
            return;
        i.p->save();
        i.p->clip(clipRect);
    }

    // Paint the children.
    RenderBlock::paintObject(i, x, y);

    // Pop the clip.
    if (i.phase == PaintPhaseForeground)
        i.p->restore();
}

void RenderMenuList::calcMinMaxWidth()
{
    if (m_optionsChanged)
        updateFromElement();

    m_minWidth = 0;
    m_maxWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else
        m_maxWidth = max(m_optionsWidth, theme()->minimumMenuListSize(style()));

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

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();
}

void RenderMenuList::showPopup()
{
    if (m_popupIsVisible)
        return;

    // Create m_innerBlock here so it ends up as the first child.
    // This is important because otherwise we might try to create m_innerBlock
    // inside the showPopup call and it would fail.
    createInnerBlock();
    if (!m_popup)
        m_popup = theme()->createPopupMenu(renderArena(), document(), this);
    RenderStyle* newStyle = new (renderArena()) RenderStyle;
    newStyle->inheritFrom(style());
    m_popup->setStyle(newStyle);
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    m_popupIsVisible = true;
    m_popup->showPopup(absoluteBoundingBoxRect(), document()->view(),
        select->optionToListIndex(select->selectedIndex()));
}

void RenderMenuList::hidePopup()
{
    if (m_popup)
        m_popup->hidePopup();
    m_popupIsVisible = false;
}

void RenderMenuList::valueChanged(unsigned listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    select->setSelectedIndex(select->listToOptionIndex(listIndex));
    select->onChange();
}

}
