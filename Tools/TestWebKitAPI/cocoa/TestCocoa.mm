/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TestCocoa.h"

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#endif

template<typename T>
static inline std::ostream& ostreamRectCommon(std::ostream& os, const T& rect)
{
    return os << "(origin = " << rect.origin << ", size = " << rect.size << ")";
}

template<typename T>
static inline std::ostream& ostreamPointCommon(std::ostream& os, const T& point)
{
    return os << "(x = " << point.x << ", y = " << point.y << ")";
}

template<typename T>
static inline std::ostream& ostreamSizeCommon(std::ostream& os, const T& size)
{
    return os << "(width = " << size.width << ", height = " << size.height << ")";
}

#if USE(CG)

std::ostream& operator<<(std::ostream& os, const CGPoint& point)
{
    return ostreamPointCommon(os, point);
}

bool operator==(const CGPoint& a, const CGPoint& b)
{
    return CGPointEqualToPoint(a, b);
}

std::ostream& operator<<(std::ostream& os, const CGSize& size)
{
    return ostreamSizeCommon(os, size);
}

bool operator==(const CGSize& a, const CGSize& b)
{
    return CGSizeEqualToSize(a, b);
}

std::ostream& operator<<(std::ostream& os, const CGRect& rect)
{
    return ostreamRectCommon(os, rect);
}

bool operator==(const CGRect& a, const CGRect& b)
{
    return CGRectEqualToRect(a, b);
}

#endif

#if PLATFORM(IOS_FAMILY)

void TestWebKitAPI::Util::instantiateUIApplicationIfNeeded(Class customApplicationClass)
{
    if (UIApplication.sharedApplication)
        return;

    UIApplicationInitialize();
    UIApplicationInstantiateSingleton(customApplicationClass ?: UIApplication.class);
}

#endif
