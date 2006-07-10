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
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "RenderPopupMenu.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderMenuList::RenderMenuList(HTMLSelectElement* element)
    : RenderFlexibleBox(element)
    , m_buttonText(0)
    , m_inner(0)
    , m_popupMenu(0)
    , m_size(element->size())
    , m_selectionChanged(true)
    , m_optionsChanged(true)
    , m_longestWidth(0)
    , m_selectedIndex(0)
{
}

void RenderMenuList::createInnerBlock()
{
    if (m_inner) {
        ASSERT(firstChild() == m_inner);
        ASSERT(!m_inner->nextSibling());
        return;
    }

    // Create an anonymous block.
    ASSERT(!firstChild());
    m_inner = createAnonymousBlock();
    m_inner->style()->setBoxFlex(1.0f);
    RenderFlexibleBox::addChild(m_inner);
}

void RenderMenuList::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    createInnerBlock();
    m_inner->addChild(newChild, beforeChild);
}

void RenderMenuList::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_inner || !m_inner) {
        RenderFlexibleBox::removeChild(oldChild);
        m_inner = 0;
    }
    else
        m_inner->removeChild(oldChild);
}

void RenderMenuList::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_buttonText)
        m_buttonText->setStyle(style);
    if (m_inner)
        m_inner->style()->setBoxFlex(1.0f);
    if (m_popupMenu) {
        RenderStyle* newStyle = new (renderArena()) RenderStyle;
        newStyle->inheritFrom(style);
        m_popupMenu->setStyle(newStyle);
    }
    setReplaced(isInline());
}

void RenderMenuList::updateFromElement()
{
    if (m_optionsChanged) {
        HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
        if (select->m_recalcListItems)
            select->recalcListItems();
        
        Vector<HTMLElement*> listItems = select->listItems();
        bool found = false;
        unsigned firstOption = listItems.size();
        int i = listItems.size();
        m_longestWidth = 0;
        while (i--)
            if (listItems[i]->hasTagName(optionTag)) {
                HTMLOptionElement* element = static_cast<HTMLOptionElement*>(listItems[i]);
                String text = element->optionText();
                float stringWidth = (text.isNull() || text.isEmpty()) ? 0 : style()->font().floatWidth(TextRun(text.impl()), TextStyle(0, 0, 0, false, false, false, false));
                if (stringWidth > m_longestWidth)
                    m_longestWidth = stringWidth;
                if (found)
                    element->m_selected = false;
                else if (element->selected()) {
                    setText(text);
                    m_selectedIndex = i;
                    found = true;
                }
                firstOption = i;
            }
        m_optionsChanged = false;
    }
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

void RenderMenuList::paintObject(PaintInfo& i, int _tx, int _ty)
{
    // Push a clip.
    if (m_inner && i.phase == PaintPhaseForeground) {
        IntRect clipRect(_tx + borderLeft(), _ty + borderTop(),
            width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop());
        if (clipRect.width() == 0 || clipRect.height() == 0)
            return;
        i.p->save();
        i.p->addClip(clipRect);
    }
    
    // Paint the children.
    RenderBlock::paintObject(i, _tx, _ty);
    
    // Pop the clip.
    if (m_inner && i.phase == PaintPhaseForeground)
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
        m_maxWidth = max((int)ceilf(m_longestWidth), theme()->minimumTextSize(style()));
    
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
    if (!m_popupMenu) {
        // Create m_inner here so it ends up as the first child.
        // This is important because otherwise we might try to create m_inner
        // inside the showPopup call and it would fail.
        createInnerBlock();
        m_popupMenu = theme()->createPopupMenu(renderArena(), document());
        RenderStyle* newStyle = new (renderArena()) RenderStyle;
        newStyle->inheritFrom(style());
        m_popupMenu->setStyle(newStyle);
        RenderFlexibleBox::addChild(m_popupMenu);
    }
    m_popupMenu->showPopup(absoluteBoundingBoxRect(), document()->view(), m_selectedIndex);
    m_popupMenu->destroy();
    m_popupMenu = 0;
}

void RenderMenuList::layout()
{
    RenderFlexibleBox::layout();
}

void RenderMenuList::updateSelection()
{
}

void RenderMenuList::valueChanged(unsigned index)
{
    m_selectedIndex = index;
    Vector<HTMLElement*> listItems = static_cast<HTMLSelectElement*>(node())->listItems();

    for (unsigned i = 0; i < listItems.size(); ++i)
        if (listItems[i]->hasTagName(optionTag) && i != index)
            static_cast<HTMLOptionElement*>(listItems[i])->m_selected = false;

    HTMLOptionElement* element = static_cast<HTMLOptionElement*>(listItems[index]);
    element->m_selected = true;
    
    setText(element->optionText());
    
    static_cast<HTMLSelectElement*>(node())->onChange();
}

}
