/*
  * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "ArgumentCodersCocoa.h"
#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/Color.h>
#import <WebCore/ColorSpace.h>
#import <WebCore/ColorSpaceCG.h>
#import <wtf/Box.h>
#import <wtf/RetainPtr.h>

static const CFStringRef kCGIndexedBaseColorSpaceKey = CFSTR("kCGIndexedBaseColorSpaceKey");
static const CFStringRef kCGLastIndexKey = CFSTR("kCGLastIndexKey");
static const CFStringRef kCGIndexedColorTableKey = CFSTR("kCGIndexedColorTableKey");
static const CFStringRef kCGColorSpaceICCData = CFSTR("kCGColorSpaceICCData");
static const CFStringRef kCGColorSpaceDisplayReferredDerivative = CFSTR("kCGColorSpaceDisplayReferredDerivative");
static const CFStringRef kCGColorSpaceSceneReferredDerivative = CFSTR("kCGColorSpaceSceneReferredDerivative");

namespace WebKit {
class CoreIPCCGColorSpace;

enum class ExtendedRangeDerivative : uint8_t {
    kNone,
    kExtendedRange,
    kExtendedRangeDisplayReferredDerivative,
    kExtendedRangeSceneReferredDerivative
};

struct ICCData {
    Vector<uint8_t> data;
    ExtendedRangeDerivative derivative { ExtendedRangeDerivative::kNone };
};

struct IndexedColorSpace {
    int8_t index;
    Vector<uint8_t> table;
    Box<CoreIPCCGColorSpace> colorSpace;
};

using CGColorSpaceSerialization = Variant<WebCore::ColorSpace, RetainPtr<CFStringRef>, WebKit::ICCData, WebKit::IndexedColorSpace>;

class CoreIPCCGColorSpace {
public:
    CoreIPCCGColorSpace(CGColorSpaceRef);
    CoreIPCCGColorSpace(CGColorSpaceSerialization);

    static CGColorSpaceSerialization serializableColorSpace(CGColorSpaceRef);
    RetainPtr<CGColorSpaceRef> toCF() const;

    CGColorSpaceSerialization m_cgColorSpace;
};

}

#endif // PLATFORM(COCOA)
