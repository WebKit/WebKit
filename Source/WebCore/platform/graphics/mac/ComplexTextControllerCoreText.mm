/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "FontCache.h"
#include "FontCascade.h"
#include <pal/spi/cocoa/CoreTextSPI.h>
#include <wtf/SoftLinking.h>
#include <wtf/WeakPtr.h>
#include <CoreText/CoreText.h>

namespace WebCore {

ComplexTextController::ComplexTextRun::ComplexTextRun(CTRunRef ctRun, const Font& font, const UChar* characters, unsigned stringLocation, unsigned stringLength, unsigned indexBegin, unsigned indexEnd)
    : m_initialAdvance(CTRunGetInitialAdvance(ctRun))
    , m_font(font)
    , m_characters(characters)
    , m_stringLength(stringLength)
    , m_indexBegin(indexBegin)
    , m_indexEnd(indexEnd)
    , m_glyphCount(CTRunGetGlyphCount(ctRun))
    , m_stringLocation(stringLocation)
    , m_isLTR(!(CTRunGetStatus(ctRun) & kCTRunStatusRightToLeft))
{
    const CFIndex* coreTextIndicesPtr = CTRunGetStringIndicesPtr(ctRun);
    Vector<CFIndex> coreTextIndices;
    if (!coreTextIndicesPtr) {
        coreTextIndices.grow(m_glyphCount);
        CTRunGetStringIndices(ctRun, CFRangeMake(0, 0), coreTextIndices.data());
        coreTextIndicesPtr = coreTextIndices.data();
    }
    m_coreTextIndices.reserveInitialCapacity(m_glyphCount);
    for (unsigned i = 0; i < m_glyphCount; ++i)
        m_coreTextIndices.uncheckedAppend(coreTextIndicesPtr[i]);

    const CGGlyph* glyphsPtr = CTRunGetGlyphsPtr(ctRun);
    Vector<CGGlyph> glyphs;
    if (!glyphsPtr) {
        glyphs.grow(m_glyphCount);
        CTRunGetGlyphs(ctRun, CFRangeMake(0, 0), glyphs.data());
        glyphsPtr = glyphs.data();
    }
    m_glyphs.reserveInitialCapacity(m_glyphCount);
    for (unsigned i = 0; i < m_glyphCount; ++i)
        m_glyphs.uncheckedAppend(glyphsPtr[i]);

    if (CTRunGetStatus(ctRun) & kCTRunStatusHasOrigins) {
        Vector<CGSize> baseAdvances(m_glyphCount);
        Vector<CGPoint> glyphOrigins(m_glyphCount);
        CTRunGetBaseAdvancesAndOrigins(ctRun, CFRangeMake(0, 0), baseAdvances.data(), glyphOrigins.data());
        m_baseAdvances.reserveInitialCapacity(m_glyphCount);
        m_glyphOrigins.reserveInitialCapacity(m_glyphCount);
        for (unsigned i = 0; i < m_glyphCount; ++i) {
            m_baseAdvances.uncheckedAppend(baseAdvances[i]);
            m_glyphOrigins.uncheckedAppend(glyphOrigins[i]);
        }
    } else {
        const CGSize* baseAdvances = CTRunGetAdvancesPtr(ctRun);
        Vector<CGSize> baseAdvancesVector;
        if (!baseAdvances) {
            baseAdvancesVector.grow(m_glyphCount);
            CTRunGetAdvances(ctRun, CFRangeMake(0, 0), baseAdvancesVector.data());
            baseAdvances = baseAdvancesVector.data();
        }
        m_baseAdvances.reserveInitialCapacity(m_glyphCount);
        for (unsigned i = 0; i < m_glyphCount; ++i)
            m_baseAdvances.uncheckedAppend(baseAdvances[i]);
    }
}

struct ProviderInfo {
    const UChar* cp;
    unsigned length;
    CFDictionaryRef attributes;
};

static const UniChar* provideStringAndAttributes(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void* refCon)
{
    ProviderInfo* info = static_cast<struct ProviderInfo*>(refCon);
    if (stringIndex < 0 || static_cast<unsigned>(stringIndex) >= info->length)
        return 0;

    *charCount = info->length - stringIndex;
    *attributes = info->attributes;
    return info->cp + stringIndex;
}

static inline bool safeCFEqual(CFTypeRef a, CFTypeRef b)
{
    return (!a && !b) || (a && b && CFEqual(a, b));
}

void ComplexTextController::collectComplexTextRunsForCharacters(const UChar* cp, unsigned length, unsigned stringLocation, const Font* font)
{
    if (!font) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, length, 0, length, m_run.ltr()));
        return;
    }

    bool isSystemFallback = false;

    UChar32 baseCharacter = 0;
    RetainPtr<CFDictionaryRef> stringAttributes;
    if (font == Font::systemFallback()) {
        // FIXME: This code path does not support small caps.
        isSystemFallback = true;

        U16_GET(cp, 0, 0, length, baseCharacter);
        font = m_font.fallbackRangesAt(0).fontForCharacter(baseCharacter);
        if (!font)
            font = &m_font.fallbackRangesAt(0).fontForFirstRange();
        stringAttributes = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, font->getCFStringAttributes(m_font.enableKerning(), font->platformData().orientation())));
        // We don't know which font should be used to render this grapheme cluster, so enable CoreText's fallback mechanism by using the CTFont which doesn't have CoreText's fallback disabled.
        CFDictionarySetValue(const_cast<CFMutableDictionaryRef>(stringAttributes.get()), kCTFontAttributeName, font->platformData().font());
    } else
        stringAttributes = font->getCFStringAttributes(m_font.enableKerning(), font->platformData().orientation());

    RetainPtr<CTLineRef> line;

    if (!m_mayUseNaturalWritingDirection || m_run.directionalOverride()) {
        const short ltrForcedEmbeddingLevelValue = 0;
        const short rtlForcedEmbeddingLevelValue = 1;
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED == 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED == 120000)
        static const void* optionKeys[] = { kCTTypesetterOptionForcedEmbeddingLevel, kCTTypesetterOptionAllowUnboundedLayout };
        static const void* ltrOptionValues[] = { CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &ltrForcedEmbeddingLevelValue), kCFBooleanTrue };
        static const void* rtlOptionValues[] = { CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &rtlForcedEmbeddingLevelValue), kCFBooleanTrue };
#else
        static const void* optionKeys[] = { kCTTypesetterOptionForcedEmbeddingLevel };
        static const void* ltrOptionValues[] = { CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &ltrForcedEmbeddingLevelValue) };
        static const void* rtlOptionValues[] = { CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &rtlForcedEmbeddingLevelValue) };
#endif
        static CFDictionaryRef ltrTypesetterOptions = CFDictionaryCreate(kCFAllocatorDefault, optionKeys, ltrOptionValues, WTF_ARRAY_LENGTH(optionKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        static CFDictionaryRef rtlTypesetterOptions = CFDictionaryCreate(kCFAllocatorDefault, optionKeys, rtlOptionValues, WTF_ARRAY_LENGTH(optionKeys), &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        ProviderInfo info = { cp, length, stringAttributes.get() };
        // FIXME: Some SDKs complain that the second parameter below cannot be null.
        IGNORE_NULL_CHECK_WARNINGS_BEGIN
        RetainPtr<CTTypesetterRef> typesetter = adoptCF(CTTypesetterCreateWithUniCharProviderAndOptions(&provideStringAndAttributes, 0, &info, m_run.ltr() ? ltrTypesetterOptions : rtlTypesetterOptions));
        IGNORE_NULL_CHECK_WARNINGS_END

        line = adoptCF(CTTypesetterCreateLine(typesetter.get(), CFRangeMake(0, 0)));
    } else {
        ProviderInfo info = { cp, length, stringAttributes.get() };

        line = adoptCF(CTLineCreateWithUniCharProvider(&provideStringAndAttributes, nullptr, &info));
    }

    m_coreTextLines.append(line.get());

    CFArrayRef runArray = CTLineGetGlyphRuns(line.get());

    CFIndex runCount = CFArrayGetCount(runArray);

    for (CFIndex r = 0; r < runCount; r++) {
        CTRunRef ctRun = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runArray, m_run.ltr() ? r : runCount - 1 - r));
        ASSERT(CFGetTypeID(ctRun) == CTRunGetTypeID());
        CFRange runRange = CTRunGetStringRange(ctRun);
        const Font* runFont = font;
        // If isSystemFallback is false, it means we disabled CoreText's font fallback mechanism, which means all the runs must use this exact font.
        // Therefore, we only need to inspect which font was actually used if isSystemFallback is true.
        if (isSystemFallback) {
            CFDictionaryRef runAttributes = CTRunGetAttributes(ctRun);
            CTFontRef runCTFont = static_cast<CTFontRef>(CFDictionaryGetValue(runAttributes, kCTFontAttributeName));
            ASSERT(runCTFont && CFGetTypeID(runCTFont) == CTFontGetTypeID());
            RetainPtr<CFTypeRef> runFontEqualityObject = FontPlatformData::objectForEqualityCheck(runCTFont);
            if (!safeCFEqual(runFontEqualityObject.get(), font->platformData().objectForEqualityCheck().get())) {
                // Begin trying to see if runFont matches any of the fonts in the fallback list.
                for (unsigned i = 0; !m_font.fallbackRangesAt(i).isNull(); ++i) {
                    runFont = m_font.fallbackRangesAt(i).fontForCharacter(baseCharacter);
                    if (!runFont)
                        continue;
                    if (safeCFEqual(runFont->platformData().objectForEqualityCheck().get(), runFontEqualityObject.get()))
                        break;
                    runFont = nullptr;
                }
                if (!runFont) {
                    RetainPtr<CFStringRef> fontName = adoptCF(CTFontCopyPostScriptName(runCTFont));
                    if (CFEqual(fontName.get(), CFSTR("LastResort"))) {
                        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, length, runRange.location, runRange.location + runRange.length, m_run.ltr()));
                        continue;
                    }
                    FontPlatformData runFontPlatformData(runCTFont, CTFontGetSize(runCTFont));
                    runFont = FontCache::singleton().fontForPlatformData(runFontPlatformData).ptr();
                }
                if (m_fallbackFonts && runFont != &m_font.primaryFont())
                    m_fallbackFonts->add(runFont);
            }
        }
        if (m_fallbackFonts && runFont != &m_font.primaryFont())
            m_fallbackFonts->add(font);

        m_complexTextRuns.append(ComplexTextRun::create(ctRun, *runFont, cp, stringLocation, length, runRange.location, runRange.location + runRange.length));
    }
}

} // namespace WebCore
