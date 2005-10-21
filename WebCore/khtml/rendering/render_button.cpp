/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
#include "render_button.h"
#include "render_text.h"
#include "htmlnames.h"
#include "html_formimpl.h"

using namespace DOM;
using namespace HTMLNames;

namespace khtml {

RenderButton::RenderButton(NodeImpl* node)
:RenderFlexibleBox(node), m_buttonText(0), m_inner(0)
{
}

RenderButton::~RenderButton()
{
}

void RenderButton::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
    if (!m_inner) {
        // Create an anonymous block.
        assert(!m_first);
        m_inner = createAnonymousBlock();
        m_inner->style()->setBoxFlex(1.0f);
        RenderFlexibleBox::addChild(m_inner);
    }
    
    m_inner->addChild(newChild, beforeChild);
}

void RenderButton::removeChild(RenderObject *oldChild)
{
    if (oldChild == m_inner || !m_inner) {
        RenderFlexibleBox::removeChild(oldChild);
        m_inner = 0;
    }
    else
        m_inner->removeChild(oldChild);
}

void RenderButton::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_buttonText)
        m_buttonText->setStyle(style);
}

void RenderButton::updateFromElement()
{
    // If we're an input element, we may need to change our button text.
    if (element()->hasTagName(inputTag)) {
        HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(element());
        DOMString value = input->value();
        if (value.isEmpty()) {
            if (m_buttonText) {
                m_buttonText->destroy();
                m_buttonText = 0;
            }
        } else {
            if (m_buttonText)
                m_buttonText->setText(value.impl());
            else {
                m_buttonText = new (renderArena()) RenderText(document(), value.impl());
                m_buttonText->setStyle(style());
                addChild(m_buttonText);
            }
        }
    }
}

}
