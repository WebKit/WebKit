/**
 * This file is part of the theme implementation for form controls in WebCore.
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
 */

#include "render_theme.h"
#include "render_style.h"
#include "htmlnames.h"
#include "html_formimpl.h"

using DOM::HTMLTags;
using DOM::HTMLInputElementImpl;

// The methods in this file are shared by all themes on every platform.

namespace khtml {

void RenderTheme::adjustStyle(RenderStyle* style)
{
    // Force inline to be inline-block
    if (style->display() == INLINE)
        style->setDisplay(INLINE_BLOCK);
    else if (style->display() == COMPACT || style->display() == RUN_IN || style->display() == LIST_ITEM)
        style->setDisplay(BLOCK);

    // Call the appropriate style adjustment method based off the appearance value.
    switch (style->appearance()) {
        case CheckboxAppearance:
            return adjustCheckboxStyle(style);
        default:
            break;
    }
}

void RenderTheme::paint(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r)
{
    // If painting is disabled, but we aren't updating control tints, then just bail.
    // If we are updating control tints, just schedule a repaint if the theme supports tinting
    // for that control.
    if (i.p->updatingControlTints()) {
        if (controlSupportsTints(o))
            o->repaint();
        return;
    }
    if (i.p->paintingDisabled())
        return;
        
    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
        case CheckboxAppearance:
            return paintCheckbox(o, i, r);
        default:
            break;
    }
}

short RenderTheme::baselinePosition(const RenderObject* o) const
{
    return o->height() + o->marginTop() + o->marginBottom();
}

bool RenderTheme::isChecked(const RenderObject* o) const
{
    if (!o->element() || !o->element()->hasTagName(HTMLTags::input()))
        return false;
    return static_cast<HTMLInputElementImpl*>(o->element())->checked();
}

bool RenderTheme::isEnabled(const RenderObject* o) const
{
    if (!o->element() || !o->element()->hasTagName(HTMLTags::input()))
        return true;
    return !static_cast<HTMLInputElementImpl*>(o->element())->disabled();
}

bool RenderTheme::isFocused(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element() == o->element()->getDocument()->focusNode();
}

bool RenderTheme::isPressed(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->active();
}

}
