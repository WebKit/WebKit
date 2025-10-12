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

#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/Color.h>
#import <WebCore/ColorSpace.h>
#import <WebCore/ColorSpaceCG.h>
#import <wtf/RetainPtr.h>

using CGColorSpaceSerialization = Variant<WebCore::ColorSpace, RetainPtr<CFStringRef>, RetainPtr<CFTypeRef>>;

namespace WebKit {
class CoreIPCCGColorSpace {
public:
    CoreIPCCGColorSpace(CGColorSpaceRef cgColorSpace)
    {
        if (auto colorSpace = WebCore::colorSpaceForCGColorSpace(cgColorSpace))
            m_cgColorSpace = *colorSpace;
        else if (RetainPtr<CFStringRef> name = CGColorSpaceGetName(cgColorSpace))
            m_cgColorSpace = WTFMove(name);
        else if (auto propertyList = adoptCF(CGColorSpaceCopyPropertyList(cgColorSpace)))
            m_cgColorSpace = WTFMove(propertyList);
        else
            // FIXME: This should be removed once we can prove only non-null cgColorSpaces.
            m_cgColorSpace = WebCore::ColorSpace::SRGB;
    }

    CoreIPCCGColorSpace(CGColorSpaceSerialization data)
        : m_cgColorSpace(data)
    {
    }

    RetainPtr<CGColorSpaceRef> toCF() const
    {
        auto colorSpace = WTF::switchOn(m_cgColorSpace,
            [](WebCore::ColorSpace colorSpace) -> RetainPtr<CGColorSpaceRef> {
                return RetainPtr { cachedNullableCGColorSpaceSingleton(colorSpace) };
            },
            [](RetainPtr<CFStringRef> name) -> RetainPtr<CGColorSpaceRef> {
                return adoptCF(CGColorSpaceCreateWithName(name.get()));
            },
            [](RetainPtr<CFTypeRef> propertyList) -> RetainPtr<CGColorSpaceRef> {
                return adoptCF(CGColorSpaceCreateWithPropertyList(propertyList.get()));
            }
        );
        if (!colorSpace) [[unlikely]]
            return nullptr;
        return colorSpace;
    }

    CGColorSpaceSerialization m_cgColorSpace;
};

}

#endif
