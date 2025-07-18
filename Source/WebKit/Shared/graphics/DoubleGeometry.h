/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(CG)
#import <CoreGraphics/CGGeometry.h>
#endif

namespace WebKit {

struct DoublePoint {
#if USE(CG)
    DoublePoint(const CGPoint& point)
        : DoublePoint(point.x, point.y)
    {
    }

    CGPoint toCG() const
    {
        return { static_cast<CGFloat>(x), static_cast<CGFloat>(y) };
    }
#endif

    DoublePoint(double x, double y)
        : x(x)
        , y(y)
    {
    }

    double x { 0 };
    double y { 0 };
};

struct DoubleSize {
#if USE(CG)
    DoubleSize(const CGSize& size)
        : DoubleSize(size.width, size.height)
    {
    }

    CGSize toCG() const
    {
        return { static_cast<CGFloat>(width), static_cast<CGFloat>(height) };
    }
#endif

    DoubleSize(double width, double height)
        : width(width)
        , height(height)
    {
    }

    double width { 0 };
    double height { 0 };
};

struct DoubleRect {
#if USE(CG)
    DoubleRect(const CGRect& rect)
        : DoubleRect(rect.origin, rect.size)
    {
    }

    CGRect toCG() const
    {
        return { origin.toCG(), size.toCG() };
    }
#endif

    DoubleRect(DoublePoint origin, DoubleSize size)
        : origin(origin)
        , size(size)
    {
    }

    DoublePoint origin;
    DoubleSize size;
};

} // namespace WebKit
