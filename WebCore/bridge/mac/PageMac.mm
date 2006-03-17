// -*- mode: c++; c-basic-offset: 4 -*-
/*
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
 */

#import "config.h"
#import "Page.h"

#import "Frame.h"
#import "IntRect.h"
#import "WebCorePageBridge.h"

namespace WebCore {

static NSRect flipGlobalRect(NSRect rect)
{
    rect.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(rect);
    return rect;
}

Page::Page(WebCorePageBridge* bridge)
    : m_frameCount(0), m_bridge(bridge)
{
    init();
}

IntRect Page::windowRect() const
{
    return enclosingIntRect(flipGlobalRect([bridge() windowFrame]));
}

void Page::setWindowRect(const IntRect& r)
{
    [bridge() setWindowFrame:flipGlobalRect(r)];
}

}
