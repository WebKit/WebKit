/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "IntRect.h"

#if !PLATFORM(MAC)
#import <UIKit/UIGeometry.h>
#endif

namespace WebCore {

id makeNSArrayElement(const IntRect& rect)
{
#if PLATFORM(MAC)
    return [NSValue valueWithRect:rect];
#else
    return [NSValue valueWithCGRect:rect];
#endif
}

#if PLATFORM(MAC) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)

IntRect::operator NSRect() const
{
    return NSMakeRect(x(), y(), width(), height());
}

IntRect enclosingIntRect(const NSRect& rect)
{
    int left = clampTo<int>(std::floor(rect.origin.x));
    int top = clampTo<int>(std::floor(rect.origin.y));
    int right = clampTo<int>(std::ceil(NSMaxX(rect)));
    int bottom = clampTo<int>(std::ceil(NSMaxY(rect)));
    return { left, top, right - left, bottom - top };
}

#endif

}
