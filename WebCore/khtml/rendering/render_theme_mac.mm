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

// The methods in this file are specific to the Mac OS X platform.

namespace khtml {

RenderTheme* theme()
{
    static RenderThemeMac macTheme;
    return &macTheme;
}

void RenderThemeMac::adjustCheckboxStyle(RenderStyle* style)
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored but we won't let ourselves get smaller than the mini checkbox.
    //                (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // padding - not honored, needs to be removed.
    // border - not honored, needs to be removed (WinIE honors it but just paints it around the control, which just looks awful.)
    
}

void RenderThemeMac::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const QRect& r)
{
    // A summary of the rules for checkbox designed to match WinIE:
    //
    // width/height - if larger than the control size, then the control should paint itself centered
    //                within the larger area.
    
}

}