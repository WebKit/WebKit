/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ComplexTextController.h"

#if USE(CORE_TEXT)

#include "Font.h"

#if defined(BUILDING_ON_LEOPARD)
// The following symbols are SPI in 10.5.
extern "C" {
void CTRunGetAdvances(CTRunRef run, CFRange range, CGSize buffer[]);
const CGSize* CTRunGetAdvancesPtr(CTRunRef run);
extern const CFStringRef kCTTypesetterOptionForcedEmbeddingLevel;
}
#endif

namespace WebCore {

ComplexTextController::ComplexTextRun::ComplexTextRun(CTRunRef ctRun, const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength)
    : m_coreTextRun(ctRun)
    , m_fontData(fontData)
    , m_characters(characters)
    , m_stringLocation(stringLocation)
    , m_stringLength(stringLength)
    , m_isMonotonic(true)
{
    m_glyphCount = CTRunGetGlyphCount(m_coreTextRun.get());
    m_coreTextIndices = CTRunGetStringIndicesPtr(m_coreTextRun.get());
    if (!m_coreTextIndices) {
        m_coreTextIndicesData.adoptCF(CFDataCreateMutable(kCFAllocatorDefault, m_glyphCount * sizeof(CFIndex)));
        CFDataIncreaseLength(m_coreTextIndicesData.get(), m_glyphCount * sizeof(CFIndex));
        m_coreTextIndices = reinterpret_cast<const CFIndex*>(CFDataGetMutableBytePtr(m_coreTextIndicesData.get()));
        CTRunGetStringIndices(m_coreTextRun.get(), CFRangeMake(0, 0), const_cast<CFIndex*>(m_coreTextIndices));
    }

    m_glyphs = CTRunGetGlyphsPtr(m_coreTextRun.get());
    if (!m_glyphs) {
        m_glyphsVector.grow(m_glyphCount);
        CTRunGetGlyphs(m_coreTextRun.get(), CFRangeMake(0, 0), m_glyphsVector.data());
        m_glyphs = m_glyphsVector.data();
    }

    m_advances = CTRunGetAdvancesPtr(m_coreTextRun.get());
    if (!m_advances) {
        m_advancesVector.grow(m_glyphCount);
        CTRunGetAdvances(m_coreTextRun.get(), CFRangeMake(0, 0), m_advancesVector.data());
        m_advances = m_advancesVector.data();
    }

}

// Missing glyphs run constructor. Core Text will not generate a run of missing glyphs, instead falling back on
// glyphs from LastResort. We want to use the primary font's missing glyph in order to match the fast text code path.
void ComplexTextController::ComplexTextRun::createTextRunFromFontDataCoreText(bool ltr)
{
    Vector<CFIndex, 16> indices;
    unsigned r = 0;
    while (r < m_stringLength) {
        indices.append(r);
        if (U_IS_SURROGATE(m_characters[r])) {
            ASSERT(r + 1 < m_stringLength);
            ASSERT(U_IS_SURROGATE_LEAD(m_characters[r]));
            ASSERT(U_IS_TRAIL(m_characters[r + 1]));
            r += 2;
        } else
            r++;
    }
    m_glyphCount = indices.size();
    if (!ltr) {
        for (unsigned r = 0, end = m_glyphCount - 1; r < m_glyphCount / 2; ++r, --end)
            std::swap(indices[r], indices[end]);
    }
    m_coreTextIndicesData.adoptCF(CFDataCreateMutable(kCFAllocatorDefault, m_glyphCount * sizeof(CFIndex)));
    CFDataAppendBytes(m_coreTextIndicesData.get(), reinterpret_cast<const UInt8*>(indices.data()), m_glyphCount * sizeof(CFIndex));
    m_coreTextIndices = reinterpret_cast<const CFIndex*>(CFDataGetBytePtr(m_coreTextIndicesData.get()));

    // Synthesize a run of missing glyphs.
    m_glyphsVector.fill(0, m_glyphCount);
    m_glyphs = m_glyphsVector.data();
    m_advancesVector.fill(CGSizeMake(m_fontData->widthForGlyph(0), 0), m_glyphCount);
    m_advances = m_advancesVector.data();
}

void ComplexTextController::collectComplexTextRunsForCharactersCoreText(const UChar* cp, unsigned length, unsigned stringLocation, const SimpleFontData* fontData)
{
    if (!fontData) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, length, m_run.ltr()));
        return;
    }

    if (m_fallbackFonts && fontData != m_font.primaryFont())
        m_fallbackFonts->add(fontData);

    RetainPtr<CFStringRef> string(AdoptCF, CFStringCreateWithCharactersNoCopy(NULL, cp, length, kCFAllocatorNull));

    RetainPtr<CFAttributedStringRef> attributedString(AdoptCF, CFAttributedStringCreate(NULL, string.get(), fontData->getCFStringAttributes(m_font.typesettingFeatures())));

    RetainPtr<CTTypesetterRef> typesetter;

    if (!m_mayUseNaturalWritingDirection || m_run.directionalOverride()) {
        static const void* optionKeys[] = { kCTTypesetterOptionForcedEmbeddingLevel };
        static const void* ltrOptionValues[] = { kCFBooleanFalse };
        static const void* rtlOptionValues[] = { kCFBooleanTrue };
        static CFDictionaryRef ltrTypesetterOptions = CFDictionaryCreate(kCFAllocatorDefault, optionKeys, ltrOptionValues, sizeof(optionKeys) / sizeof(*optionKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        static CFDictionaryRef rtlTypesetterOptions = CFDictionaryCreate(kCFAllocatorDefault, optionKeys, rtlOptionValues, sizeof(optionKeys) / sizeof(*optionKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        typesetter.adoptCF(CTTypesetterCreateWithAttributedStringAndOptions(attributedString.get(), m_run.ltr() ? ltrTypesetterOptions : rtlTypesetterOptions));
    } else
        typesetter.adoptCF(CTTypesetterCreateWithAttributedString(attributedString.get()));

    RetainPtr<CTLineRef> line(AdoptCF, CTTypesetterCreateLine(typesetter.get(), CFRangeMake(0, 0)));

    CFArrayRef runArray = CTLineGetGlyphRuns(line.get());

    CFIndex runCount = CFArrayGetCount(runArray);

    for (CFIndex r = 0; r < runCount; r++) {
        CTRunRef ctRun = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runArray, r));
        ASSERT(CFGetTypeID(ctRun) == CTRunGetTypeID());
        m_complexTextRuns.append(ComplexTextRun::create(ctRun, fontData, cp, stringLocation, length));
    }
}

} // namespace WebCore

#endif // USE(CORE_TEXT)
