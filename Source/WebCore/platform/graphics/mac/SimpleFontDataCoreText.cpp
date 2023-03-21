/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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

#include "config.h"
#include "Font.h"

#include <CoreText/CoreText.h>
#include <pal/spi/cf/CoreTextSPI.h>

namespace WebCore {

static CTParagraphStyleRef paragraphStyleWithCompositionLanguageNone()
{
    static LazyNeverDestroyed<RetainPtr<CTParagraphStyleRef>> paragraphStyle;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        paragraphStyle.construct(CTParagraphStyleCreate(nullptr, 0));
        CTParagraphStyleSetCompositionLanguage(paragraphStyle.get().get(), kCTCompositionLanguageNone);
    });
    return paragraphStyle.get().get();
}

static CFNumberRef zeroValue()
{
    static LazyNeverDestroyed<RetainPtr<CFNumberRef>> zeroValue;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        const float zero = 0;
        zeroValue.construct(adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &zero)));
    });
    return zeroValue.get().get();
}

RetainPtr<CFDictionaryRef> Font::getCFStringAttributes(bool enableKerning, FontOrientation orientation, const AtomString& locale) const
{
    CFTypeRef keys[5];
    CFTypeRef values[5];

    keys[0] = kCTFontAttributeName;
    values[0] = platformData().ctFont();
    size_t count = 1;

    RetainPtr<CFStringRef> localeString;
    if (!locale.isEmpty()) {
        localeString = locale.string().createCFString();
        keys[count] = kCTLanguageAttributeName;
        values[count] = localeString.get();
        ++count;
    }

    keys[count] = kCTParagraphStyleAttributeName;
    values[count] = paragraphStyleWithCompositionLanguageNone();
    ++count;

    if (!enableKerning) {
        keys[count] = kCTKernAttributeName;
        values[count] = zeroValue();
        ++count;
    }

    if (orientation == FontOrientation::Vertical) {
        keys[count] = kCTVerticalFormsAttributeName;
        values[count] = kCFBooleanTrue;
        ++count;
    }

    ASSERT(count <= std::size(keys));

    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

} // namespace WebCore
