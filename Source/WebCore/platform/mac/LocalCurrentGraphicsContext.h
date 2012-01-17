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

#include "GraphicsContext.h"
#include <wtf/Noncopyable.h>

#if USE(SKIA)
#include "skia/ext/skia_utils_mac.h"
#endif

OBJC_CLASS NSGraphicsContext;

namespace WebCore {

// This class automatically saves and restores the current NSGraphicsContext for
// functions which call out into AppKit and rely on the currentContext being set
class LocalCurrentGraphicsContext {
    WTF_MAKE_NONCOPYABLE(LocalCurrentGraphicsContext);
public:
    LocalCurrentGraphicsContext(GraphicsContext* graphicsContext);
    ~LocalCurrentGraphicsContext();
    CGContextRef cgContext();
private:
    GraphicsContext* m_savedGraphicsContext;
    NSGraphicsContext* m_savedNSGraphicsContext;
#if USE(SKIA)
    gfx::SkiaBitLocker m_skiaBitLocker;
#endif
};

class ContextContainer {
    WTF_MAKE_NONCOPYABLE(ContextContainer);
public:
#if USE(SKIA)
    ContextContainer(GraphicsContext*);
    
    // This synchronizes the CGContext to reflect the current SkCanvas state.
    // The implementation may not return the same CGContext each time.
    CGContextRef context() { return m_skiaBitLocker.cgContext(); }
#else
    ContextContainer(GraphicsContext* graphicsContext)
        : m_graphicsContext(graphicsContext->platformContext())
    {
    }

    CGContextRef context() { return m_graphicsContext; }
#endif
private:
#if USE(SKIA)
    gfx::SkiaBitLocker m_skiaBitLocker;
#else
    PlatformGraphicsContext* m_graphicsContext;
#endif
};

}
