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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "LocalCurrentGraphicsContext.h"

#include <AppKit/NSGraphicsContext.h>

#if USE(SKIA)
#include "platform_canvas.h"
#include "PlatformContextSkia.h"
#endif

namespace WebCore {

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(GraphicsContext* graphicsContext)
#if USE(SKIA)
    : m_skiaBitLocker(graphicsContext->platformContext()->canvas())
#endif
{
    m_savedGraphicsContext = graphicsContext;
    graphicsContext->save();

    CGContextRef cgContext = this->cgContext();
    if (cgContext == [[NSGraphicsContext currentContext] graphicsPort]) {
        m_savedNSGraphicsContext = 0;
        return;
    }
    
    m_savedNSGraphicsContext = [[NSGraphicsContext currentContext] retain];
    NSGraphicsContext* newContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES];
    [NSGraphicsContext setCurrentContext:newContext];
}

LocalCurrentGraphicsContext::~LocalCurrentGraphicsContext()
{
    m_savedGraphicsContext->restore();

    if (m_savedNSGraphicsContext) {
        [NSGraphicsContext setCurrentContext:m_savedNSGraphicsContext];
        [m_savedNSGraphicsContext release];
    }
}

CGContextRef LocalCurrentGraphicsContext::cgContext()
{
#if USE(SKIA)
    // This synchronizes the CGContext to reflect the current SkCanvas state.
    // The implementation may not return the same CGContext each time.
    CGContextRef cgContext = m_skiaBitLocker.cgContext();
#else
    CGContextRef cgContext = m_savedGraphicsContext->platformContext();
#endif
    return cgContext;
}

#if USE(SKIA)
ContextContainer::ContextContainer(GraphicsContext* graphicsContext) 
    : m_skiaBitLocker(graphicsContext->platformContext()->canvas())
{
}
#endif

}
