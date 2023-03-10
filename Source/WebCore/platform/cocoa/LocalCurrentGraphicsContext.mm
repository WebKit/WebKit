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
#endif
#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(GraphicsContext& context, bool isFlipped)
    : LocalCurrentGraphicsContext(context.hasPlatformContext() ? context.platformContext() : nullptr, isFlipped)
{
}

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(CGContextRef cgContext, bool isFlipped)
    : m_cgContext(cgContext)
{
    if (m_cgContext)
        CGContextSaveGState(m_cgContext.get());
    applyState(isFlipped);
}

LocalCurrentGraphicsContext::~LocalCurrentGraphicsContext()
{
    restoreState();
    if (m_cgContext)
        CGContextRestoreGState(m_cgContext.get());
}

#if USE(APPKIT)

void LocalCurrentGraphicsContext::applyState(bool isFlipped)
{
    if (m_cgContext == [[NSGraphicsContext currentContext] CGContext])
        return;
    m_savedNSContext = RetainPtr { [NSGraphicsContext currentContext] };
    RetainPtr<NSGraphicsContext> newContext;
    if (m_cgContext)
        newContext = [NSGraphicsContext graphicsContextWithCGContext:m_cgContext.get() flipped:isFlipped];
    [NSGraphicsContext setCurrentContext:newContext.get()];
}

void LocalCurrentGraphicsContext::restoreState()
{
    if (!m_savedNSContext.has_value())
        return;
    [NSGraphicsContext setCurrentContext:m_savedNSContext->get()];
}

#endif

#if PLATFORM(IOS_FAMILY)

void LocalCurrentGraphicsContext::applyState(bool)
{
    if (m_cgContext == UIGraphicsGetCurrentContext())
        return;
    UIGraphicsPushContext(m_cgContext.get());
    m_didSetGlobalContext = true;
}

void LocalCurrentGraphicsContext::restoreState()
{
    if (!m_didSetGlobalContext)
        return;
    UIGraphicsPopContext();
}

#endif


}

