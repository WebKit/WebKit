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

#import "render_theme_mac.h"

#import "render_style.h"
#import "render_canvas.h"
#import "dom_elementimpl.h"
#import "khtmlview.h"

// The methods in this file are specific to the Mac OS X platform.

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};
    
namespace khtml {

RenderTheme* theme()
{
    static RenderThemeMac macTheme;
    return &macTheme;
}

RenderThemeMac::RenderThemeMac()
{
    checkbox = nil;
}

void RenderThemeMac::adjustRepaintRect(const RenderObject* o, QRect& r)
{
    if (o->style()->appearance() == CheckboxAppearance) {
        // Since we query the prototype cell, we need to update its state to match.
        setCheckboxCellState(o, r);
    
        // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
        // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
        r = inflateRect(r, checkboxSizes()[[checkbox controlSize]], checkboxMargins());
    }
}

QRect RenderThemeMac::inflateRect(const QRect& r, int size, const int* margins) const
{
    // Only do the inflation if the available width/height are too small.  Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = r.width() - (size + margins[leftMargin] + margins[rightMargin]);
    int heightDelta = r.height() - (size + margins[topMargin] + margins[bottomMargin]);
    QRect result(r);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[leftMargin]);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[topMargin]);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

void RenderThemeMac::updateCheckedState(NSCell* cell, const RenderObject* o)
{
    bool oldChecked = [cell state] == NSOnState;
    bool checked = isChecked(o);
    if (checked != oldChecked)
        [cell setState:checked ? NSOnState : NSOffState];
}

void RenderThemeMac::updateEnabledState(NSCell* cell, const RenderObject* o)
{
    bool oldEnabled = [cell isEnabled];
    bool enabled = isEnabled(o);
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];
}

void RenderThemeMac::updateFocusedState(NSCell* cell, const RenderObject* o)
{
    // FIXME: Need to add a key window test here, or the element will look
    // focused even when in the background.
    bool oldFocused = [cell showsFirstResponder];
    bool focused = (o->element() && o->document()->focusNode() == o->element()) && (o->style()->outlineStyleIsAuto());
    if (focused != oldFocused)
        [cell setShowsFirstResponder:focused];
}

void RenderThemeMac::updatePressedState(NSCell* cell, const RenderObject* o)
{
    bool oldPressed = [cell isHighlighted];
    bool pressed = (o->element() && o->element()->active());
    if (pressed != oldPressed)
        [cell setHighlighted:pressed];
}

short RenderThemeMac::baselinePosition(const RenderObject* o) const
{
    if (o->style()->appearance() == CheckboxAppearance)
        return o->marginTop() + o->height() - 2; // The baseline is 2px up from the bottom of the checkbox in AppKit.
    return RenderTheme::baselinePosition(o);
}

bool RenderThemeMac::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;
    
    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxAppearance)
        return isChecked(o);
    
    // For now assume other controls have tint if enabled.
    return true;
}

NSControlSize RenderThemeMac::controlSizeForFont(RenderStyle* style) const
{
    int fontSize = style->fontSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

int RenderThemeMac::sizeForFont(RenderStyle* style) const
{
    return checkboxSizes()[controlSizeForFont(style)];
}

void RenderThemeMac::setControlSize(NSCell* cell, const int* sizes, int minSize)
{
    NSControlSize size;
    if (minSize >= sizes[NSRegularControlSize])
        size = NSRegularControlSize;
    else if (minSize >= sizes[NSSmallControlSize])
        size = NSSmallControlSize;
    else
        size = NSMiniControlSize;
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
}

void RenderThemeMac::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r)
{
    // Determine the width and height needed for the control and prepare the cell for painting.
    setCheckboxCellState(o, r);
    
    // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
    // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
    QRect inflatedRect = inflateRect(r, checkboxSizes()[[checkbox controlSize]], checkboxMargins());
    [checkbox drawWithFrame:NSRect(inflatedRect) inView:o->canvas()->view()->getDocumentView()];
    [checkbox setControlView: nil];
}

const int* RenderThemeMac::checkboxSizes() const
{
    static const int sizes[3] = { 14, 12, 10 };
    return sizes;
}

const int* RenderThemeMac::checkboxMargins() const
{
    static const int margins[3][4] = 
    {
        { 3, 4, 4, 2 },
        { 4, 3, 3, 3 },
        { 4, 3, 3, 3 },
    };
    return margins[[checkbox controlSize]];
}

void RenderThemeMac::setCheckboxCellState(const RenderObject* o, const QRect& r)
{
    if (!checkbox) {
        checkbox = [[NSButtonCell alloc] init];
        [checkbox setButtonType:NSSwitchButton];
        [checkbox setTitle:nil];
    }
    
    // Set the control size based off the rectangle we're painting into.
    setControlSize(checkbox, checkboxSizes(), kMin(r.width(), r.height()));
    
    // Update the various states we respond to.
    updateCheckedState(checkbox, o);
    updateEnabledState(checkbox, o);
    updatePressedState(checkbox, o);
    updateFocusedState(checkbox, o);
}

}
