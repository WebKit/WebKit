/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FontFamilySpecificationCoreText.h"

#include <pal/spi/cocoa/CoreTextSPI.h>
#include "FontCache.h"
#include "FontSelector.h"

#include <CoreText/CoreText.h>

namespace WebCore {

FontFamilySpecificationCoreText::FontFamilySpecificationCoreText(CTFontDescriptorRef fontDescriptor)
    : m_fontDescriptor(fontDescriptor)
{
}

FontFamilySpecificationCoreText::~FontFamilySpecificationCoreText() = default;

FontRanges FontFamilySpecificationCoreText::fontRanges(const FontDescription& fontDescription) const
{
    auto size = fontDescription.computedSize();

    auto font = adoptCF(CTFontCreateWithFontDescriptor(m_fontDescriptor.get(), size, nullptr));

    auto fontForSynthesisComputation = font;
#if USE_PLATFORM_SYSTEM_FALLBACK_LIST
    if (auto physicalFont = adoptCF(CTFontCopyPhysicalFont(font.get())))
        fontForSynthesisComputation = physicalFont;
#endif

    font = preparePlatformFont(font.get(), fontDescription, nullptr, nullptr, { }, fontDescription.computedSize());

    bool syntheticBold, syntheticOblique;
    std::tie(syntheticBold, syntheticOblique) = computeNecessarySynthesis(fontForSynthesisComputation.get(), fontDescription).boldObliquePair();

    FontPlatformData fontPlatformData(font.get(), size, false, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant(), fontDescription.textRenderingMode());

    return FontRanges(FontCache::singleton().fontForPlatformData(fontPlatformData));
}

}
