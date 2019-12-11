/*
 * Copyright (C) 2005, 2006, 2012 Apple Inc. All rights reserved.
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
#include <pal/spi/cocoa/CoreTextSPI.h>

namespace WebCore {

CFDictionaryRef Font::getCFStringAttributes(bool enableKerning, FontOrientation orientation) const
{
    auto& attributesDictionary = enableKerning ? m_kernedCFStringAttributes : m_nonKernedCFStringAttributes;
    if (attributesDictionary)
        return attributesDictionary.get();

    attributesDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFDictionarySetValue(attributesDictionary.get(), kCTFontAttributeName, platformData().ctFont());
    auto paragraphStyle = adoptCF(CTParagraphStyleCreate(nullptr, 0));
    CTParagraphStyleSetCompositionLanguage(paragraphStyle.get(), kCTCompositionLanguageNone);
    CFDictionarySetValue(attributesDictionary.get(), kCTParagraphStyleAttributeName, paragraphStyle.get());

    if (!enableKerning) {
        const float zero = 0;
        static CFNumberRef zeroKerningValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &zero);
        CFDictionarySetValue(attributesDictionary.get(), kCTKernAttributeName, zeroKerningValue);
    }

    if (orientation == FontOrientation::Vertical)
        CFDictionarySetValue(attributesDictionary.get(), kCTVerticalFormsAttributeName, kCFBooleanTrue);

    return attributesDictionary.get();
}

#if HAVE(DISALLOWABLE_USER_INSTALLED_FONTS)
bool Font::isUserInstalledFont() const
{
    auto isUserInstalledFont = adoptCF(static_cast<CFBooleanRef>(CTFontCopyAttribute(getCTFont(), kCTFontUserInstalledAttribute)));
    return isUserInstalledFont && CFBooleanGetValue(isUserInstalledFont.get());
}
#endif

} // namespace WebCore
