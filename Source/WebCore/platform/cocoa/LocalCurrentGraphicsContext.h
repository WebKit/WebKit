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

#pragma once

#include "GraphicsContext.h"
#include <wtf/Noncopyable.h>

#if PLATFORM(COCOA)

#if USE(APPKIT)
OBJC_CLASS NSGraphicsContext;
#endif

namespace WebCore {

// This class automatically saves and restores the current NSGraphicsContext for
// functions which call out into AppKit and rely on the currentContext being set
class LocalCurrentGraphicsContext {
    WTF_MAKE_NONCOPYABLE(LocalCurrentGraphicsContext);
public:
    WEBCORE_EXPORT LocalCurrentGraphicsContext(GraphicsContext&);
    WEBCORE_EXPORT ~LocalCurrentGraphicsContext();
    CGContextRef cgContext() { return m_savedGraphicsContext.platformContext(); }
private:
    GraphicsContext& m_savedGraphicsContext;
#if USE(APPKIT)
    RetainPtr<NSGraphicsContext> m_savedNSGraphicsContext;
#endif
    bool m_didSetGraphicsContext { false };
};

class ContextContainer {
    WTF_MAKE_NONCOPYABLE(ContextContainer);
public:
    ContextContainer(GraphicsContext& graphicsContext)
        : m_graphicsContext(graphicsContext.platformContext())
    {
    }

    CGContextRef context() { return m_graphicsContext; }
private:
    CGContextRef m_graphicsContext;
};

}

#endif // PLATFORM(COCOA)
