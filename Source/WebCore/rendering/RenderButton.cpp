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
#include <wtf/IsoMallocInlines.h>

#if PLATFORM(IOS)
#include "RenderThemeIOS.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderButton);

RenderButton::RenderButton(HTMLFormControlElement& element, RenderStyle&& style)
    : RenderFlexibleBox(element, WTFMove(style))
{
}

RenderButton::~RenderButton() = default;

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

void RenderButton::addChild(RenderPtr<RenderObject> newChild, RenderObject* beforeChild)
{
    if (!m_inner) {
        // Create an anonymous block.
        ASSERT(!firstChild());
        auto newInner = createAnonymousBlock(style().display());
        updateAnonymousChildStyle(*newInner, newInner->mutableStyle());
        m_inner = makeWeakPtr(*newInner);
        RenderFlexibleBox::addChild(WTFMove(newInner));
    }    
    m_inner->addChild(WTFMove(newChild), beforeChild);
}

RenderPtr<RenderObject> RenderButton::takeChild(RenderObject& oldChild)
{
    // m_inner should be the only child, but checking for direct children who
    // are not m_inner prevents security problems when that assumption is
    // violated.
    if (&oldChild == m_inner || !m_inner || oldChild.parent() == this) {
        ASSERT(&oldChild == m_inner || !m_inner);
        return RenderFlexibleBox::takeChild(oldChild);
    }
    return m_inner->takeChild(oldChild);
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
    if (!m_buttonText && str.isEmpty())
        return;

    if (!m_buttonText) {
        auto newButtonText = createRenderer<RenderTextFragment>(document(), str);
        m_buttonText = makeWeakPtr(*newButtonText);
        addChild(WTFMove(newButtonText));
        return;
    }

    if (!str.isEmpty()) {
        m_buttonText->setText(str.impl());
        return;
    }
    m_buttonText->removeFromParentAndDestroy();
}

String RenderButton::text() const
{
    if (m_buttonText)
        return m_buttonText->text();
    return { };
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
