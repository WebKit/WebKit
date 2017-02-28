/**
 * Copyright (C) 2005 Apple Inc.
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
#include "RenderTheme.h"
#include "StyleInheritedData.h"

#if PLATFORM(IOS)
#include "RenderThemeIOS.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderButton::RenderButton(HTMLFormControlElement& element, RenderStyle&& style)
    : RenderFlexibleBox(element, WTFMove(style))
    , m_buttonText(0)
    , m_inner(0)
{
}

RenderButton::~RenderButton()
{
}

HTMLFormControlElement& RenderButton::formControlElement() const
{
    return downcast<HTMLFormControlElement>(nodeForNonAnonymous());
}

bool RenderButton::canBeSelectionLeaf() const
{
    return formControlElement().hasEditableStyle();
}

bool RenderButton::hasLineIfEmpty() const
{
    return is<HTMLInputElement>(formControlElement());
}

void RenderButton::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!m_inner) {
        // Create an anonymous block.
        ASSERT(!firstChild());
        m_inner = createAnonymousBlock(style().display());
        updateAnonymousChildStyle(*m_inner, m_inner->mutableStyle());
        RenderFlexibleBox::addChild(m_inner);
    }
    
    m_inner->addChild(newChild, beforeChild);
}

void RenderButton::removeChild(RenderObject& oldChild)
{
    // m_inner should be the only child, but checking for direct children who
    // are not m_inner prevents security problems when that assumption is
    // violated.
    if (&oldChild == m_inner || !m_inner || oldChild.parent() == this) {
        ASSERT(&oldChild == m_inner || !m_inner);
        RenderFlexibleBox::removeChild(oldChild);
        m_inner = nullptr;
    } else
        m_inner->removeChild(oldChild);
}
    
void RenderButton::updateAnonymousChildStyle(const RenderObject& child, RenderStyle& childStyle) const
{
    ASSERT_UNUSED(child, !m_inner || &child == m_inner);
    
    childStyle.setFlexGrow(1.0f);
    // min-width: 0; is needed for correct shrinking.
    childStyle.setMinWidth(Length(0, Fixed));
    // Use margin:auto instead of align-items:center to get safe centering, i.e.
    // when the content overflows, treat it the same as align-items: flex-start.
    childStyle.setMarginTop(Length());
    childStyle.setMarginBottom(Length());
    childStyle.setFlexDirection(style().flexDirection());
    childStyle.setJustifyContent(style().justifyContent());
    childStyle.setFlexWrap(style().flexWrap());
    childStyle.setAlignItems(style().alignItems());
    childStyle.setAlignContent(style().alignContent());
}

void RenderButton::updateFromElement()
{
    // If we're an input element, we may need to change our button text.
    if (is<HTMLInputElement>(formControlElement())) {
        HTMLInputElement& input = downcast<HTMLInputElement>(formControlElement());
        String value = input.valueWithDefault();
        setText(value);
    }
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
            m_buttonText = new RenderTextFragment(document(), str);
            addChild(m_buttonText);
        }
    }
}

String RenderButton::text() const
{
    return m_buttonText ? m_buttonText->text() : 0;
}

bool RenderButton::canHaveGeneratedChildren() const
{
    // Input elements can't have generated children, but button elements can. We'll
    // write the code assuming any other button types that might emerge in the future
    // can also have children.
    return !is<HTMLInputElement>(formControlElement());
}

LayoutRect RenderButton::controlClipRect(const LayoutPoint& additionalOffset) const
{
    // Clip to the padding box to at least give content the extra padding space.
    return LayoutRect(additionalOffset.x() + borderLeft(), additionalOffset.y() + borderTop(), width() - borderLeft() - borderRight(), height() - borderTop() - borderBottom());
}

#if PLATFORM(IOS)
void RenderButton::layout()
{
    RenderFlexibleBox::layout();

    // FIXME: We should not be adjusting styles during layout. See <rdar://problem/7675493>.
    RenderThemeIOS::adjustRoundBorderRadius(mutableStyle(), *this);
}
#endif

} // namespace WebCore
