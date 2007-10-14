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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderButton.h"

#include "Document.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "RenderTextFragment.h"

namespace WebCore {

using namespace HTMLNames;

RenderButton::RenderButton(Node* node)
    : RenderFlexibleBox(node)
    , m_buttonText(0)
    , m_inner(0)
{
}

void RenderButton::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!m_inner) {
        // Create an anonymous block.
        ASSERT(!firstChild());
        m_inner = createAnonymousBlock();
        m_inner->style()->setBoxFlex(1.0f);
        RenderFlexibleBox::addChild(m_inner);
    }
    
    m_inner->addChild(newChild, beforeChild);
}

void RenderButton::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_inner || !m_inner) {
        RenderFlexibleBox::removeChild(oldChild);
        m_inner = 0;
    } else
        m_inner->removeChild(oldChild);
}

void RenderButton::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_buttonText)
        m_buttonText->setStyle(style);
    if (m_inner) // RenderBlock handled updating the anonymous block's style.
        m_inner->style()->setBoxFlex(1.0f);
    setReplaced(isInline());
}

void RenderButton::updateFromElement()
{
    // If we're an input element, we may need to change our button text.
    if (element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element());
        String value = input->valueWithDefault();
        setText(value);
    }
}

bool RenderButton::canHaveChildren() const
{
    // Input elements can't have children, but button elements can.  We'll
    // write the code assuming any other button types that might emerge in the future
    // can also have children.
    return !element()->hasTagName(inputTag);
}

void RenderButton::setText(const String& str)
{
    if (str.isEmpty()) {
        if (m_buttonText) {
            m_buttonText->destroy();
            m_buttonText = 0;
        }
    } else {
        if (m_buttonText)
            m_buttonText->setText(str.impl());
        else {
            m_buttonText = new (renderArena()) RenderTextFragment(document(), str.impl());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
    }
}

void RenderButton::updateBeforeAfterContent(RenderStyle::PseudoId type)
{
    if (m_inner)
        m_inner->updateBeforeAfterContentForContainer(type, this);
    else
        updateBeforeAfterContentForContainer(type, this);
}

IntRect RenderButton::controlClipRect(int tx, int ty) const
{
    // Clip to the padding box to at least give content the extra padding space.
    return IntRect(tx + borderLeft(), ty + borderTop(), m_width - borderLeft() - borderRight(), m_height - borderTop() - borderBottom());
}


} // namespace WebCore
