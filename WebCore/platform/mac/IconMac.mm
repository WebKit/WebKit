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
 *
 */

#import "config.h"
#import "Icon.h"

#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "PlatformString.h"
#import <AppKit/NSImage.h>

namespace WebCore {

Icon::Icon()
    : m_nsImage(0)
{
}

Icon::~Icon()
{
    [m_nsImage release];
}

PassRefPtr<Icon> Icon::newIconForFile(const String& filename)
{
    NSImage* fileIcon = [[[NSWorkspace sharedWorkspace] iconForFile:filename] retain];
    if (!fileIcon)
        return PassRefPtr<Icon>(0);
    
    PassRefPtr<Icon> icon(new Icon());

    icon->m_nsImage = fileIcon;
    
    // Need this because WebCore uses AppKit's flipped coordinate system.
    [icon->m_nsImage setFlipped:YES];
    
    return icon;
}

void Icon::paint(GraphicsContext* context, const IntRect& r)
{
    if (context->paintingDisabled())
        return;
    
    LocalCurrentGraphicsContext localCurrentGC(context);
    
    [m_nsImage drawInRect:r
        fromRect:NSMakeRect(0, 0, [m_nsImage size].width, [m_nsImage size].height)
        operation:NSCompositeSourceOver fraction:1.0];
}

}
