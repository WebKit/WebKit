/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ComplexTextController.h"

#if USE(ATSUI)

#include "CharacterNames.h"
#include "Font.h"
#include "ShapeArabic.h"

#ifdef __LP64__
// ATSUTextInserted() is SPI in 64-bit.
extern "C" {
OSStatus ATSUTextInserted(ATSUTextLayout iTextLayout,  UniCharArrayOffset iInsertionLocation, UniCharCount iInsertionLength);
}
#endif

using namespace WTF::Unicode;

namespace WebCore {

OSStatus ComplexTextController::ComplexTextRun::overrideLayoutOperation(ATSULayoutOperationSelector, ATSULineRef atsuLineRef, URefCon refCon, void*, ATSULayoutOperationCallbackStatus* callbackStatus)
{
    ComplexTextRun* complexTextRun = reinterpret_cast<ComplexTextRun*>(refCon);
    OSStatus status;
    ItemCount count;
    ATSLayoutRecord *layoutRecords;

    status = ATSUDirectGetLayoutDataArrayPtrFromLineRef(atsuLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, true, reinterpret_cast<void**>(&layoutRecords), &count);
    if (status != noErr) {
        *callbackStatus = kATSULayoutOperationCallbackStatusContinue;
        return status;
    }

    count--;
    ItemCount j = 0;
    CFIndex indexOffset = 0;

    if (complexTextRun->m_directionalOverride) {
        j++;
        count -= 2;
        indexOffset = -1;
    }

    complexTextRun->m_glyphCount = count;
    complexTextRun->m_glyphsVector.grow(count);
    complexTextRun->m_glyphs = complexTextRun->m_glyphsVector.data();
    complexTextRun->m_advancesVector.grow(count);
    complexTextRun->m_advances = complexTextRun->m_advancesVector.data();
    complexTextRun->m_indices.grow(count);

    CGFloat lastX = FixedToFloat(layoutRecords[j].realPos);

    for (ItemCount i = 0; i < count; ++i, ++j) {
        complexTextRun->m_glyphsVector[i] = layoutRecords[j].glyphID;
        complexTextRun->m_indices[i] = layoutRecords[j].originalOffset / 2 + indexOffset;
        CGFloat x = FixedToFloat(layoutRecords[j + 1].realPos);
        complexTextRun->m_advancesVector[i] = CGSizeMake(x - lastX, 0);
        lastX = x;
    }

    status = ATSUDirectReleaseLayoutDataArrayPtr(atsuLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, reinterpret_cast<void**>(&layoutRecords));
    *callbackStatus = kATSULayoutOperationCallbackStatusContinue;
    return noErr;
}

static inline bool isArabicLamWithAlefLigature(UChar c)
{
    return c >= 0xfef5 && c <= 0xfefc;
}

static void shapeArabic(const UChar* source, UChar* dest, unsigned totalLength)
{
    unsigned shapingStart = 0;
    while (shapingStart < totalLength) {
        unsigned shapingEnd;
        // We do not want to pass a Lam with Alef ligature followed by a space to the shaper,
        // since we want to be able to identify this sequence as the result of shaping a Lam
        // followed by an Alef and padding with a space.
        bool foundLigatureSpace = false;
        for (shapingEnd = shapingStart; !foundLigatureSpace && shapingEnd < totalLength - 1; ++shapingEnd)
            foundLigatureSpace = isArabicLamWithAlefLigature(source[shapingEnd]) && source[shapingEnd + 1] == ' ';
        shapingEnd++;

        UErrorCode shapingError = U_ZERO_ERROR;
        unsigned charsWritten = shapeArabic(source + shapingStart, shapingEnd - shapingStart, dest + shapingStart, shapingEnd - shapingStart, U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_FIXED_SPACES_NEAR, &shapingError);

        if (U_SUCCESS(shapingError) && charsWritten == shapingEnd - shapingStart) {
            for (unsigned j = shapingStart; j < shapingEnd - 1; ++j) {
                if (isArabicLamWithAlefLigature(dest[j]) && dest[j + 1] == ' ')
                    dest[++j] = zeroWidthSpace;
            }
            if (foundLigatureSpace) {
                dest[shapingEnd] = ' ';
                shapingEnd++;
            } else if (isArabicLamWithAlefLigature(dest[shapingEnd - 1])) {
                // u_shapeArabic quirk: if the last two characters in the source string are a Lam and an Alef,
                // the space is put at the beginning of the string, despite U_SHAPE_LENGTH_FIXED_SPACES_NEAR.
                ASSERT(dest[shapingStart] == ' ');
                dest[shapingStart] = zeroWidthSpace;
            }
        } else {
            // Something went wrong. Abandon shaping and just copy the rest of the buffer.
            LOG_ERROR("u_shapeArabic failed(%d)", shapingError);
            shapingEnd = totalLength;
            memcpy(dest + shapingStart, source + shapingStart, (shapingEnd - shapingStart) * sizeof(UChar));
        }
        shapingStart = shapingEnd;
    }
}

ComplexTextController::ComplexTextRun::ComplexTextRun(ATSUTextLayout atsuTextLayout, const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr, bool directionalOverride)
    : m_fontData(fontData)
    , m_characters(characters)
    , m_stringLocation(stringLocation)
    , m_stringLength(stringLength)
    , m_directionalOverride(directionalOverride)
{
    OSStatus status;

    status = ATSUSetTextLayoutRefCon(atsuTextLayout, reinterpret_cast<URefCon>(this));

    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;

    Boolean rtl = !ltr;

    Vector<UChar, 256> substituteCharacters;
    bool shouldCheckForMirroring = !ltr && !fontData->m_ATSUMirrors;
    bool shouldCheckForArabic = !fontData->shapesArabic();
    bool shouldShapeArabic = false;

    bool mirrored = false;
    for (size_t i = 0; i < stringLength; ++i) {
        if (shouldCheckForMirroring) {
            UChar mirroredChar = u_charMirror(characters[i]);
            if (mirroredChar != characters[i]) {
                if (!mirrored) {
                    mirrored = true;
                    substituteCharacters.grow(stringLength);
                    memcpy(substituteCharacters.data(), characters, stringLength * sizeof(UChar));
                    ATSUTextMoved(atsuTextLayout, substituteCharacters.data());
                }
                substituteCharacters[i] = mirroredChar;
            }
        }
        if (shouldCheckForArabic && isArabicChar(characters[i])) {
            shouldCheckForArabic = false;
            shouldShapeArabic = true;
        }
    }

    if (shouldShapeArabic) {
        Vector<UChar, 256> shapedArabic(stringLength);
        shapeArabic(substituteCharacters.isEmpty() ? characters : substituteCharacters.data(), shapedArabic.data(), stringLength);
        substituteCharacters.swap(shapedArabic);
        ATSUTextMoved(atsuTextLayout, substituteCharacters.data());
    }

    if (directionalOverride) {
        UChar override = ltr ? leftToRightOverride : rightToLeftOverride;
        if (substituteCharacters.isEmpty()) {
            substituteCharacters.grow(stringLength + 2);
            substituteCharacters[0] = override;
            memcpy(substituteCharacters.data() + 1, characters, stringLength * sizeof(UChar));
            substituteCharacters[stringLength + 1] = popDirectionalFormatting;
            ATSUTextMoved(atsuTextLayout, substituteCharacters.data());
        } else {
            substituteCharacters.prepend(override);
            substituteCharacters.append(popDirectionalFormatting);
        }
        ATSUTextInserted(atsuTextLayout, 0, 2);
    }

    ATSULayoutOperationOverrideSpecifier overrideSpecifier;
    overrideSpecifier.operationSelector = kATSULayoutOperationPostLayoutAdjustment;
    overrideSpecifier.overrideUPP = overrideLayoutOperation;

    ATSUAttributeTag tags[] = { kATSULineLayoutOptionsTag, kATSULineDirectionTag, kATSULayoutOperationOverrideTag };
    ByteCount sizes[] = { sizeof(ATSLineLayoutOptions), sizeof(Boolean), sizeof(ATSULayoutOperationOverrideSpecifier) };
    ATSUAttributeValuePtr values[] = { &lineLayoutOptions, &rtl, &overrideSpecifier };

    status = ATSUSetLayoutControls(atsuTextLayout, 3, tags, sizes, values);

    ItemCount boundsCount;
    status = ATSUGetGlyphBounds(atsuTextLayout, 0, 0, 0, m_stringLength, kATSUseFractionalOrigins, 0, 0, &boundsCount);

    status = ATSUDisposeTextLayout(atsuTextLayout);
}

ComplexTextController::ComplexTextRun::ComplexTextRun(const SimpleFontData* fontData, const UChar* characters, unsigned stringLocation, size_t stringLength, bool ltr)
    : m_fontData(fontData)
    , m_characters(characters)
    , m_stringLocation(stringLocation)
    , m_stringLength(stringLength)
{
    m_indices.reserveCapacity(stringLength);
    unsigned r = 0;
    while (r < stringLength) {
        m_indices.uncheckedAppend(r);
        if (U_IS_SURROGATE(characters[r])) {
            ASSERT(r + 1 < stringLength);
            ASSERT(U_IS_SURROGATE_LEAD(characters[r]));
            ASSERT(U_IS_TRAIL(characters[r + 1]));
            r += 2;
        } else
            r++;
    }
    m_glyphCount = m_indices.size();
    if (!ltr) {
        for (unsigned r = 0, end = m_glyphCount - 1; r < m_glyphCount / 2; ++r, --end)
            std::swap(m_indices[r], m_indices[end]);
    }

    m_glyphsVector.fill(0, m_glyphCount);
    m_glyphs = m_glyphsVector.data();
    m_advancesVector.fill(CGSizeMake(fontData->widthForGlyph(0), 0), m_glyphCount);
    m_advances = m_advancesVector.data();
}

static bool fontHasMirroringInfo(ATSUFontID fontID)
{
    ByteCount propTableSize;
    OSStatus status = ATSFontGetTable(fontID, 'prop', 0, 0, 0, &propTableSize);
    if (status == noErr)    // naively assume that if a 'prop' table exists then it contains mirroring info
        return true;
    else if (status != kATSInvalidFontTableAccess) // anything other than a missing table is logged as an error
        LOG_ERROR("ATSFontGetTable failed (%d)", static_cast<int>(status));

    return false;
}

static void disableLigatures(const SimpleFontData* fontData, TextRenderingMode textMode)
{
    // Don't be too aggressive: if the font doesn't contain 'a', then assume that any ligatures it contains are
    // in characters that always go through ATSUI, and therefore allow them. Geeza Pro is an example.
    // See bugzilla 5166.
    if (textMode == OptimizeLegibility || textMode == GeometricPrecision || fontData->platformData().allowsLigatures())
        return;

    ATSUFontFeatureType featureTypes[] = { kLigaturesType };
    ATSUFontFeatureSelector featureSelectors[] = { kCommonLigaturesOffSelector };
    OSStatus status = ATSUSetFontFeatures(fontData->m_ATSUStyle, 1, featureTypes, featureSelectors);
    if (status != noErr)
        LOG_ERROR("ATSUSetFontFeatures failed (%d) -- ligatures remain enabled", static_cast<int>(status));
}

static void initializeATSUStyle(const SimpleFontData* fontData, TextRenderingMode textMode)
{
    if (fontData->m_ATSUStyleInitialized)
        return;

    ATSUFontID fontID = fontData->platformData().m_atsuFontID;
    if (!fontID) {
        LOG_ERROR("unable to get ATSUFontID for %p", fontData->platformData().font());
        return;
    }

    OSStatus status = ATSUCreateStyle(&fontData->m_ATSUStyle);
    if (status != noErr)
        LOG_ERROR("ATSUCreateStyle failed (%d)", static_cast<int>(status));

    Fixed fontSize = FloatToFixed(fontData->platformData().m_size);
    Fract kerningInhibitFactor = FloatToFract(1);
    static CGAffineTransform verticalFlip = CGAffineTransformMakeScale(1, -1);

    ByteCount styleSizes[4] = { sizeof(fontSize), sizeof(fontID), sizeof(verticalFlip), sizeof(kerningInhibitFactor) };
    ATSUAttributeTag styleTags[4] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag, kATSUKerningInhibitFactorTag };
    ATSUAttributeValuePtr styleValues[4] = { &fontSize, &fontID, &verticalFlip, &kerningInhibitFactor };

    bool allowKerning = textMode == OptimizeLegibility || textMode == GeometricPrecision;
    status = ATSUSetAttributes(fontData->m_ATSUStyle, allowKerning ? 3 : 4, styleTags, styleSizes, styleValues);
    if (status != noErr)
        LOG_ERROR("ATSUSetAttributes failed (%d)", static_cast<int>(status));

    fontData->m_ATSUMirrors = fontHasMirroringInfo(fontID);

    disableLigatures(fontData, textMode);

    fontData->m_ATSUStyleInitialized = true;
}

void ComplexTextController::collectComplexTextRunsForCharacters(const UChar* cp, unsigned length, unsigned stringLocation, const SimpleFontData* fontData)
{
    if (!fontData) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, length, m_run.ltr()));
        return;
    }

    if (m_fallbackFonts && fontData != m_font.primaryFont())
        m_fallbackFonts->add(fontData);

    initializeATSUStyle(fontData, m_font.fontDescription().textRenderingMode());

    OSStatus status;
    ATSUTextLayout atsuTextLayout;
    UniCharCount runLength = length;

    status = ATSUCreateTextLayoutWithTextPtr(cp, 0, length, length, 1, &runLength, &fontData->m_ATSUStyle, &atsuTextLayout);
    if (status != noErr) {
        LOG_ERROR("ATSUCreateTextLayoutWithTextPtr failed with error %d", static_cast<int>(status));
        return;
    }
    m_complexTextRuns.append(ComplexTextRun::create(atsuTextLayout, fontData, cp, stringLocation, length, m_run.ltr(), m_run.directionalOverride()));
}

} // namespace WebCore

#endif // USE(ATSUI)
