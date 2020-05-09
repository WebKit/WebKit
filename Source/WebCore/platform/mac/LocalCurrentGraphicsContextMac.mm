/*
 * Copyright (C) 2006-2020 Apple Inc.
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

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(GraphicsContext& graphicsContext)
    : m_savedGraphicsContext(graphicsContext)
{
    m_savedGraphicsContext.save();

    if (!m_savedGraphicsContext.hasPlatformContext()) {
        WTFLogAlways("LocalCurrentGraphicsContext is not setting the global context because the provided GraphicsContext does not have a platform context (likely display list recording)");
        return;
    }

    CGContextRef cgContext = this->cgContext();
    if (cgContext == [[NSGraphicsContext currentContext] CGContext])
        return;

    m_savedNSGraphicsContext = [NSGraphicsContext currentContext];
    NSGraphicsContext* newContext = [NSGraphicsContext graphicsContextWithCGContext:cgContext flipped:YES];
    [NSGraphicsContext setCurrentContext:newContext];
    m_didSetGraphicsContext = true;
}

LocalCurrentGraphicsContext::~LocalCurrentGraphicsContext()
{
    if (m_didSetGraphicsContext)
        [NSGraphicsContext setCurrentContext:m_savedNSGraphicsContext.get()];

    m_savedGraphicsContext.restore();
}

}

#endif // USE(APPKIT)
