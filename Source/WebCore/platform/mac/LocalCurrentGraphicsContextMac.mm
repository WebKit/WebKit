/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
 */

#import "config.h"
#import "LocalCurrentGraphicsContext.h"

#if USE(APPKIT)

#import <AppKit/NSGraphicsContext.h>

namespace WebCore {

LocalCurrentContextSaver::LocalCurrentContextSaver(CGContextRef cgContext, bool isFlipped)
{
    if (!cgContext) {
        ASSERT_NOT_REACHED();
        [NSGraphicsContext setCurrentContext:nil];
        return;
    }

    if (cgContext == [[NSGraphicsContext currentContext] CGContext])
        return;

    lazyInitialize(m_savedNSGraphicsContext, RetainPtr { [NSGraphicsContext currentContext] });
    NSGraphicsContext* newContext = [NSGraphicsContext graphicsContextWithCGContext:cgContext flipped:isFlipped];
    [NSGraphicsContext setCurrentContext:newContext];
    m_didSetGraphicsContext = true;
}

LocalCurrentContextSaver::~LocalCurrentContextSaver()
{
    if (m_didSetGraphicsContext)
        [NSGraphicsContext setCurrentContext:m_savedNSGraphicsContext.get()];
}

}

#endif // USE(APPKIT)
