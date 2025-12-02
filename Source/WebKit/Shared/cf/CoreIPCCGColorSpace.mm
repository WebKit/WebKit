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

#import "config.h"
#import "CoreIPCCGColorSpace.h"

#if PLATFORM(COCOA)

#import "CoreIPCTypes.h"

namespace WebKit {

CGColorSpaceSerialization CoreIPCCGColorSpace::serializableColorSpace(CGColorSpaceRef cgColorSpace)
{
    if (auto colorSpace = WebCore::colorSpaceForCGColorSpace(cgColorSpace))
        return *colorSpace;

    if (RetainPtr<CFStringRef> name = CGColorSpaceGetName(cgColorSpace))
        return WTFMove(name);

    if (auto propertyList = adoptCF(CGColorSpaceCopyPropertyList(cgColorSpace))) {
        if (auto data = dynamic_cf_cast<CFDataRef>(propertyList.get()))
            return ICCData { makeVector(data), ExtendedRangeDerivative::kNone };

        if (RetainPtr dictionary = dynamic_cf_cast<CFDictionaryRef>(propertyList.get())) {
            if (RetainPtr data = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(dictionary.get(), kCGColorSpaceICCData))) {
                ICCData iccdata;
                iccdata.data = makeVector(data.get());
                iccdata.derivative = ExtendedRangeDerivative::kExtendedRange;
                if (CFDictionaryContainsKey(dictionary.get(), kCGColorSpaceDisplayReferredDerivative))
                    iccdata.derivative = ExtendedRangeDerivative::kExtendedRangeDisplayReferredDerivative;
                if (CFDictionaryContainsKey(dictionary.get(), kCGColorSpaceSceneReferredDerivative))
                    iccdata.derivative = ExtendedRangeDerivative::kExtendedRangeSceneReferredDerivative;
                return iccdata;
            }

            if (RetainPtr table = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(dictionary.get(), kCGIndexedColorTableKey))) {
                int8_t value;
                RetainPtr lastIndex = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(dictionary.get(), kCGLastIndexKey));
                if (lastIndex) {
                    CFNumberGetValue(lastIndex.get(), kCFNumberSInt8Type, &value);
                    RetainPtr colorSpace = CGColorSpaceGetBaseColorSpace(cgColorSpace);
                    return IndexedColorSpace { value, makeVector(table.get()), Box<CoreIPCCGColorSpace>::create(serializableColorSpace(colorSpace.get())) };
                }
            }
        }
    }
    // FIXME: This should be removed once we can prove only non-null cgColorSpaces.
    return WebCore::ColorSpace::SRGB;
}

CoreIPCCGColorSpace::CoreIPCCGColorSpace(CGColorSpaceRef cgColorSpace)
    : m_cgColorSpace(serializableColorSpace(cgColorSpace))
{
}

CoreIPCCGColorSpace::CoreIPCCGColorSpace(CGColorSpaceSerialization data)
    : m_cgColorSpace(data)
{
}

RetainPtr<CGColorSpaceRef> CoreIPCCGColorSpace::toCF() const
{
    auto colorSpace = WTF::switchOn(m_cgColorSpace,
    [](WebCore::ColorSpace colorSpace) -> RetainPtr<CGColorSpaceRef> {
        return RetainPtr { cachedNullableCGColorSpaceSingleton(colorSpace) };
    },
    [](RetainPtr<CFStringRef> name) -> RetainPtr<CGColorSpaceRef> {
        return adoptCF(CGColorSpaceCreateWithName(name.get()));
    },
    [](const ICCData& iccdata) -> RetainPtr<CGColorSpaceRef> {
        if (iccdata.derivative == ExtendedRangeDerivative::kNone)
            return adoptCF(CGColorSpaceCreateWithPropertyList(toCFData(iccdata.data).get()));

        const void* keys[] = { kCGColorSpaceICCData, kCGColorSpaceExtendedRange, iccdata.derivative == ExtendedRangeDerivative::kExtendedRangeDisplayReferredDerivative ? kCGColorSpaceDisplayReferredDerivative : kCGColorSpaceSceneReferredDerivative };
        RetainPtr data = toCFData(iccdata.data);
        const void* vals[] = { data.get(), kCFBooleanTrue, kCFBooleanTrue };
        RetainPtr propertyList = adoptCF(CFDictionaryCreate(NULL,
            (const void **)keys,
            (const void **)vals,
            iccdata.derivative == ExtendedRangeDerivative::kExtendedRange ? 2 : 3,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks));
        return adoptCF(CGColorSpaceCreateWithPropertyList(propertyList.get()));
    },
    [](const IndexedColorSpace& colorSpace) -> RetainPtr<CGColorSpaceRef> {
        RetainPtr innerColorSpace = colorSpace.colorSpace->toCF();
        RetainPtr innerPropertyList = adoptCF(CGColorSpaceCopyPropertyList(innerColorSpace.get()));
        RetainPtr lastIndex = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type, &colorSpace.index));
        RetainPtr table = adoptCF(CGColorSpaceCreateWithPropertyList(toCFData(colorSpace.table).get()));
        const void* keys[] = { kCGIndexedBaseColorSpaceKey, kCGLastIndexKey, kCGIndexedColorTableKey };
        const void* vals[] = { innerPropertyList.get(), lastIndex.get(), table.get() };
        RetainPtr propertyList = adoptCF(CFDictionaryCreate(NULL,
            (const void **)keys,
            (const void **)vals,
            3,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks));
        return adoptCF(CGColorSpaceCreateWithPropertyList(propertyList.get()));
    });
    if (!colorSpace) [[unlikely]]
        return nullptr;
    return colorSpace;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
