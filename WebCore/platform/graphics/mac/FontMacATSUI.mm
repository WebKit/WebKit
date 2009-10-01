/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#import "config.h"
#import "Font.h"

#if USE(ATSUI)

#import "CharacterNames.h"
#import "GraphicsContext.h"
#import "Logging.h"
#import "ShapeArabic.h"
#import "SimpleFontData.h"
#import <AppKit/NSGraphicsContext.h>
#import <wtf/OwnArrayPtr.h>

#define SYNTHETIC_OBLIQUE_ANGLE 14

#ifdef __LP64__
#define URefCon void*
#else
#define URefCon UInt32
#endif

using namespace std;

namespace WebCore {

struct ATSULayoutParameters : Noncopyable
{
    ATSULayoutParameters(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts = 0)
        : m_run(run)
        , m_font(0)
        , m_hasSyntheticBold(false)
        , m_syntheticBoldPass(false)
        , m_padPerSpace(0)
        , m_fallbackFonts(fallbackFonts)
    {
    }

    ~ATSULayoutParameters()
    {
        ATSUDisposeTextLayout(m_layout);
    }

    void initialize(const Font*, const GraphicsContext* = 0);

    const TextRun& m_run;
    
    const Font* m_font;
    
    ATSUTextLayout m_layout;
    OwnArrayPtr<const SimpleFontData*> m_fonts;
    
    OwnArrayPtr<UChar> m_charBuffer;
    bool m_hasSyntheticBold;
    bool m_syntheticBoldPass;
    float m_padPerSpace;
    HashSet<const SimpleFontData*>* m_fallbackFonts;
};

static TextRun copyRunForDirectionalOverrideIfNecessary(const TextRun& run, OwnArrayPtr<UChar>& charactersWithOverride)
{
    if (!run.directionalOverride())
        return run;

    charactersWithOverride.set(new UChar[run.length() + 2]);
    charactersWithOverride[0] = run.rtl() ? rightToLeftOverride : leftToRightOverride;
    memcpy(&charactersWithOverride[1], run.data(0), sizeof(UChar) * run.length());
    charactersWithOverride[run.length() + 1] = popDirectionalFormatting;

    TextRun result = run;
    result.setText(charactersWithOverride.get(), run.length() + 2);
    return result;
}

static bool fontHasMirroringInfo(ATSUFontID fontID)
{
    ByteCount propTableSize;
    OSStatus status = ATSFontGetTable(fontID, 'prop', 0, 0, 0, &propTableSize);
    if (status == noErr)    // naively assume that if a 'prop' table exists then it contains mirroring info
        return true;
    else if (status != kATSInvalidFontTableAccess) // anything other than a missing table is logged as an error
        LOG_ERROR("ATSFontGetTable failed (%d)", status);

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
        LOG_ERROR("ATSUSetFontFeatures failed (%d) -- ligatures remain enabled", status);
}

static void initializeATSUStyle(const SimpleFontData* fontData, TextRenderingMode textMode)
{
    if (fontData->m_ATSUStyleInitialized)
        return;

    ATSUFontID fontID = fontData->platformData().m_atsuFontID;
    if (!fontID) {
        LOG_ERROR("unable to get ATSUFontID for %@", fontData->platformData().font());
        return;
    }

    OSStatus status = ATSUCreateStyle(&fontData->m_ATSUStyle);
    if (status != noErr)
        // Who knows how many ATSU functions will crash when passed a NULL style...
        LOG_ERROR("ATSUCreateStyle failed (%d)", status);

    CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);
    if (fontData->platformData().m_syntheticOblique)
        transform = CGAffineTransformConcat(transform, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0));
    Fixed fontSize = FloatToFixed(fontData->platformData().m_size);
    ByteCount styleSizes[4] = { sizeof(Fixed), sizeof(ATSUFontID), sizeof(CGAffineTransform), sizeof(Fract) };
    
    bool allowKerning = textMode == OptimizeLegibility || textMode == GeometricPrecision;
    if (!allowKerning) {
        // Turn off automatic kerning until it is supported in the CG code path (bug 6136)
        Fract kerningInhibitFactor = FloatToFract(1.0);
        ATSUAttributeTag styleTags[4] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag, kATSUKerningInhibitFactorTag };
        ATSUAttributeValuePtr styleValues[4] = { &fontSize, &fontID, &transform, &kerningInhibitFactor };
        status = ATSUSetAttributes(fontData->m_ATSUStyle, 4, styleTags, styleSizes, styleValues);
        if (status != noErr)
            LOG_ERROR("ATSUSetAttributes failed (%d)", status);
    } else {
        ATSUAttributeTag styleTags[3] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag };
        ATSUAttributeValuePtr styleValues[3] = { &fontSize, &fontID, &transform, };
        status = ATSUSetAttributes(fontData->m_ATSUStyle, 3, styleTags, styleSizes, styleValues);
        if (status != noErr)
            LOG_ERROR("ATSUSetAttributes failed (%d)", status);
    }

    fontData->m_ATSUMirrors = fontHasMirroringInfo(fontID);

    // Turn off ligatures such as 'fi' to match the CG code path's behavior, until bug 6135 is fixed.
    disableLigatures(fontData, textMode);

    fontData->m_ATSUStyleInitialized = true;
}

static OSStatus overrideLayoutOperation(ATSULayoutOperationSelector, ATSULineRef iLineRef, URefCon iRefCon, void*, ATSULayoutOperationCallbackStatus* oCallbackStatus)
{
    ATSULayoutParameters* params = reinterpret_cast<ATSULayoutParameters*>(iRefCon);
    OSStatus status;
    ItemCount count;
    ATSLayoutRecord *layoutRecords;

    if (params->m_run.applyWordRounding()) {
        status = ATSUDirectGetLayoutDataArrayPtrFromLineRef(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, true, (void **)&layoutRecords, &count);
        if (status != noErr) {
            *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
            return status;
        }
        
        Fixed lastNativePos = 0;
        float lastAdjustedPos = 0;
        const UChar* characters = params->m_charBuffer ? params->m_charBuffer.get() : params->m_run.characters();
        const SimpleFontData** renderers = params->m_fonts.get();
        const SimpleFontData* renderer;
        const SimpleFontData* lastRenderer = 0;
        ByteCount offset = layoutRecords[0].originalOffset;
        UChar nextCh = *(UChar *)(((char *)characters)+offset);
        bool shouldRound = false;
        bool syntheticBoldPass = params->m_syntheticBoldPass;
        Fixed syntheticBoldOffset = 0;
        bool hasExtraSpacing = (params->m_font->letterSpacing() || params->m_font->wordSpacing() || params->m_run.padding()) && !params->m_run.spacingDisabled();
        float padding = params->m_run.padding();
        // In the CoreGraphics code path, the rounding hack is applied in logical order.
        // Here it is applied in visual left-to-right order, which may be better.
        ItemCount lastRoundingChar = 0;
        ItemCount i;
        for (i = 1; i < count; i++) {
            bool isLastChar = i == count - 1;
            renderer = renderers[offset / 2];
            float width;
            if (nextCh == zeroWidthSpace || Font::treatAsZeroWidthSpace(nextCh) && !Font::treatAsSpace(nextCh)) {
                width = 0;
                layoutRecords[i-1].glyphID = renderer->spaceGlyph();
            } else {
                width = FixedToFloat(layoutRecords[i].realPos - lastNativePos);
                if (renderer != lastRenderer && width) {
                    lastRenderer = renderer;
                    // The CoreGraphics interpretation of NSFontAntialiasedIntegerAdvancementsRenderingMode seems
                    // to be "round each glyph's width to the nearest integer". This is not the same as ATSUI
                    // does in any of its device-metrics modes.
                    shouldRound = renderer->platformData().roundsGlyphAdvances();
                    if (syntheticBoldPass)
                        syntheticBoldOffset = FloatToFixed(renderer->syntheticBoldOffset());
                    if (params->m_fallbackFonts && renderer != params->m_font->primaryFont())
                        params->m_fallbackFonts->add(renderer);
                }
                if (shouldRound)
                    width = roundf(width);
                width += renderer->syntheticBoldOffset();
                if (renderer->pitch() == FixedPitch ? width == renderer->spaceWidth() : (layoutRecords[i-1].flags & kATSGlyphInfoIsWhiteSpace))
                    width = renderer->adjustedSpaceWidth();
            }
            lastNativePos = layoutRecords[i].realPos;

            if (hasExtraSpacing) {
                if (width && params->m_font->letterSpacing())
                    width +=params->m_font->letterSpacing();
                if (Font::treatAsSpace(nextCh)) {
                    if (params->m_run.padding()) {
                        if (padding < params->m_padPerSpace) {
                            width += padding;
                            padding = 0;
                        } else {
                            width += params->m_padPerSpace;
                            padding -= params->m_padPerSpace;
                        }
                    }
                    if (offset != 0 && !Font::treatAsSpace(*((UChar *)(((char *)characters)+offset) - 1)) && params->m_font->wordSpacing())
                        width += params->m_font->wordSpacing();
                }
            }

            UChar ch = nextCh;
            offset = layoutRecords[i].originalOffset;
            // Use space for nextCh at the end of the loop so that we get inside the rounding hack code.
            // We won't actually round unless the other conditions are satisfied.
            nextCh = isLastChar ? ' ' : *(UChar *)(((char *)characters)+offset);

            if (Font::isRoundingHackCharacter(ch))
                width = ceilf(width);
            lastAdjustedPos = lastAdjustedPos + width;
            if (Font::isRoundingHackCharacter(nextCh) && (!isLastChar || params->m_run.applyRunRounding())){
                if (params->m_run.ltr())
                    lastAdjustedPos = ceilf(lastAdjustedPos);
                else {
                    float roundingWidth = ceilf(lastAdjustedPos) - lastAdjustedPos;
                    Fixed rw = FloatToFixed(roundingWidth);
                    ItemCount j;
                    for (j = lastRoundingChar; j < i; j++)
                        layoutRecords[j].realPos += rw;
                    lastRoundingChar = i;
                    lastAdjustedPos += roundingWidth;
                }
            }
            if (syntheticBoldPass) {
                if (syntheticBoldOffset)
                    layoutRecords[i-1].realPos += syntheticBoldOffset;
                else
                    layoutRecords[i-1].glyphID = renderer->spaceGlyph();
            }
            layoutRecords[i].realPos = FloatToFixed(lastAdjustedPos);
        }
        
        status = ATSUDirectReleaseLayoutDataArrayPtr(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void **)&layoutRecords);
    }
    *oCallbackStatus = kATSULayoutOperationCallbackStatusHandled;
    return noErr;
}

static inline bool isArabicLamWithAlefLigature(UChar c)
{
    return c >= 0xfef5 && c <= 0xfefc;
}

static void shapeArabic(const UChar* source, UChar* dest, unsigned totalLength, unsigned shapingStart)
{
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

void ATSULayoutParameters::initialize(const Font* font, const GraphicsContext* graphicsContext)
{
    m_font = font;
    
    const SimpleFontData* fontData = font->primaryFont();
    m_fonts.set(new const SimpleFontData*[m_run.length()]);
    if (font->isSmallCaps())
        m_charBuffer.set(new UChar[m_run.length()]);
    
    ATSUTextLayout layout;
    OSStatus status;
    ATSULayoutOperationOverrideSpecifier overrideSpecifier;
    
    initializeATSUStyle(fontData, m_font->fontDescription().textRenderingMode());
    
    // FIXME: This is currently missing the following required features that the CoreGraphics code path has:
    // - \n, \t, and nonbreaking space render as a space.

    UniCharCount runLength = m_run.length();

    if (m_charBuffer)
        memcpy(m_charBuffer.get(), m_run.characters(), runLength * sizeof(UChar));
    
    status = ATSUCreateTextLayoutWithTextPtr(
            (m_charBuffer ? m_charBuffer.get() : m_run.characters()),
            0,        // offset
            runLength,      // length
            runLength,    // total length
            1,              // styleRunCount
            &runLength,     // length of style run
            &fontData->m_ATSUStyle, 
            &layout);
    if (status != noErr)
        LOG_ERROR("ATSUCreateTextLayoutWithTextPtr failed(%d)", status);
    m_layout = layout;
    ATSUSetTextLayoutRefCon(m_layout, (URefCon)this);

    // FIXME: There are certain times when this method is called, when we don't have access to a GraphicsContext
    // measuring text runs with floatWidthForComplexText is one example.
    // ATSUI requires that we pass a valid CGContextRef to it when specifying kATSUCGContextTag (crashes when passed 0)
    // ATSUI disables sub-pixel rendering if kATSUCGContextTag is not specified!  So we're in a bind.
    // Sometimes [[NSGraphicsContext currentContext] graphicsPort] may return the wrong (or no!) context.  Nothing we can do about it (yet).
    CGContextRef cgContext = graphicsContext ? graphicsContext->platformContext() : (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    
    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;
    Boolean rtl = m_run.rtl();
    overrideSpecifier.operationSelector = kATSULayoutOperationPostLayoutAdjustment;
    overrideSpecifier.overrideUPP = overrideLayoutOperation;
    ATSUAttributeTag tags[] = { kATSUCGContextTag, kATSULineLayoutOptionsTag, kATSULineDirectionTag, kATSULayoutOperationOverrideTag };
    ByteCount sizes[] = { sizeof(CGContextRef), sizeof(ATSLineLayoutOptions), sizeof(Boolean), sizeof(ATSULayoutOperationOverrideSpecifier) };
    ATSUAttributeValuePtr values[] = { &cgContext, &lineLayoutOptions, &rtl, &overrideSpecifier };
    
    status = ATSUSetLayoutControls(layout, (m_run.applyWordRounding() ? 4 : 3), tags, sizes, values);
    if (status != noErr)
        LOG_ERROR("ATSUSetLayoutControls failed(%d)", status);

    status = ATSUSetTransientFontMatching(layout, YES);
    if (status != noErr)
        LOG_ERROR("ATSUSetTransientFontMatching failed(%d)", status);

    m_hasSyntheticBold = false;
    ATSUFontID ATSUSubstituteFont;
    UniCharArrayOffset substituteOffset = 0;
    UniCharCount substituteLength;
    UniCharArrayOffset lastOffset;
    const SimpleFontData* substituteFontData = 0;

    while (substituteOffset < runLength) {
        // FIXME: Using ATSUMatchFontsToText() here results in several problems: the CSS font family list is not necessarily followed for the 2nd
        // and onwards unmatched characters; segmented fonts do not work correctly; behavior does not match the simple text and Uniscribe code
        // paths. Change this function to use Font::glyphDataForCharacter() for each character instead. 
        lastOffset = substituteOffset;
        status = ATSUMatchFontsToText(layout, substituteOffset, kATSUToTextEnd, &ATSUSubstituteFont, &substituteOffset, &substituteLength);
        if (status == kATSUFontsMatched || status == kATSUFontsNotMatched) {
            const FontData* fallbackFontData = m_font->fontDataForCharacters(m_run.characters() + substituteOffset, substituteLength);
            substituteFontData = fallbackFontData ? fallbackFontData->fontDataForCharacter(m_run[0]) : 0;
            if (substituteFontData) {
                initializeATSUStyle(substituteFontData, m_font->fontDescription().textRenderingMode());
                if (substituteFontData->m_ATSUStyle)
                    ATSUSetRunStyle(layout, substituteFontData->m_ATSUStyle, substituteOffset, substituteLength);
            } else
                substituteFontData = fontData;
        } else {
            substituteOffset = runLength;
            substituteLength = 0;
        }

        bool shapedArabic = false;
        bool isSmallCap = false;
        UniCharArrayOffset firstSmallCap = 0;
        const SimpleFontData *r = fontData;
        UniCharArrayOffset i;
        for (i = lastOffset;  ; i++) {
            if (i == substituteOffset || i == substituteOffset + substituteLength) {
                if (isSmallCap) {
                    isSmallCap = false;
                    initializeATSUStyle(r->smallCapsFontData(m_font->fontDescription()), m_font->fontDescription().textRenderingMode());
                    ATSUSetRunStyle(layout, r->smallCapsFontData(m_font->fontDescription())->m_ATSUStyle, firstSmallCap, i - firstSmallCap);
                }
                if (i == substituteOffset && substituteLength > 0)
                    r = substituteFontData;
                else
                    break;
            }
            if (!shapedArabic && WTF::Unicode::isArabicChar(m_run[i]) && !r->shapesArabic()) {
                shapedArabic = true;
                if (!m_charBuffer) {
                    m_charBuffer.set(new UChar[runLength]);
                    memcpy(m_charBuffer.get(), m_run.characters(), i * sizeof(UChar));
                    ATSUTextMoved(layout, m_charBuffer.get());
                }
                shapeArabic(m_run.characters(), m_charBuffer.get(), runLength, i);
            }
            if (m_run.rtl() && !r->m_ATSUMirrors) {
                UChar mirroredChar = u_charMirror(m_run[i]);
                if (mirroredChar != m_run[i]) {
                    if (!m_charBuffer) {
                        m_charBuffer.set(new UChar[runLength]);
                        memcpy(m_charBuffer.get(), m_run.characters(), runLength * sizeof(UChar));
                        ATSUTextMoved(layout, m_charBuffer.get());
                    }
                    m_charBuffer[i] = mirroredChar;
                }
            }
            if (m_font->isSmallCaps()) {
                const SimpleFontData* smallCapsData = r->smallCapsFontData(m_font->fontDescription());
                UChar c = m_charBuffer[i];
                UChar newC;
                if (U_GET_GC_MASK(c) & U_GC_M_MASK)
                    m_fonts[i] = isSmallCap ? smallCapsData : r;
                else if (!u_isUUppercase(c) && (newC = u_toupper(c)) != c) {
                    m_charBuffer[i] = newC;
                    if (!isSmallCap) {
                        isSmallCap = true;
                        firstSmallCap = i;
                    }
                    m_fonts[i] = smallCapsData;
                } else {
                    if (isSmallCap) {
                        isSmallCap = false;
                        initializeATSUStyle(smallCapsData, m_font->fontDescription().textRenderingMode());
                        ATSUSetRunStyle(layout, smallCapsData->m_ATSUStyle, firstSmallCap, i - firstSmallCap);
                    }
                    m_fonts[i] = r;
                }
            } else
                m_fonts[i] = r;
            if (m_fonts[i]->syntheticBoldOffset())
                m_hasSyntheticBold = true;
        }
        substituteOffset += substituteLength;
    }
    if (m_run.padding()) {
        float numSpaces = 0;
        unsigned k;
        for (k = 0; k < runLength; k++)
            if (Font::treatAsSpace(m_run[k]))
                numSpaces++;

        if (numSpaces == 0)
            m_padPerSpace = 0;
        else
            m_padPerSpace = ceilf(m_run.padding() / numSpaces);
    } else
        m_padPerSpace = 0;
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& point, int h, int from, int to) const
{
    OwnArrayPtr<UChar> charactersWithOverride;
    TextRun adjustedRun = copyRunForDirectionalOverrideIfNecessary(run, charactersWithOverride);
    if (run.directionalOverride()) {
        from++;
        to++;
    }

    ATSULayoutParameters params(adjustedRun);
    params.initialize(this);

    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    
    OSStatus status = ATSUGetGlyphBounds(params.m_layout, 0, 0, from, to - from, kATSUseFractionalOrigins, 1, &firstGlyphBounds, &actualNumBounds);
    if (status != noErr || actualNumBounds != 1) {
        static ATSTrapezoid zeroTrapezoid = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
        firstGlyphBounds = zeroTrapezoid;
    }
    
    float beforeWidth = min(FixedToFloat(firstGlyphBounds.lowerLeft.x), FixedToFloat(firstGlyphBounds.upperLeft.x));
    float afterWidth = max(FixedToFloat(firstGlyphBounds.lowerRight.x), FixedToFloat(firstGlyphBounds.upperRight.x));
    
    FloatRect rect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);

    return rect;
}

void Font::drawComplexText(GraphicsContext* graphicsContext, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    OSStatus status;
    
    int drawPortionLength = to - from;
    OwnArrayPtr<UChar> charactersWithOverride;
    TextRun adjustedRun = copyRunForDirectionalOverrideIfNecessary(run, charactersWithOverride);
    if (run.directionalOverride())
        from++;

    ATSULayoutParameters params(adjustedRun);
    params.initialize(this, graphicsContext);
    
    // ATSUI can't draw beyond -32768 to +32767 so we translate the CTM and tell ATSUI to draw at (0, 0).
    CGContextRef context = graphicsContext->platformContext();
    CGContextTranslateCTM(context, point.x(), point.y());

    IntSize shadowSize;
    int shadowBlur;
    Color shadowColor;
    graphicsContext->getShadow(shadowSize, shadowBlur, shadowColor);

    bool hasSimpleShadow = graphicsContext->textDrawingMode() == cTextFill && shadowColor.isValid() && !shadowBlur;
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        graphicsContext->clearShadow();
        Color fillColor = graphicsContext->fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        graphicsContext->setFillColor(shadowFillColor);
        CGContextTranslateCTM(context, shadowSize.width(), shadowSize.height());
        status = ATSUDrawText(params.m_layout, from, drawPortionLength, 0, 0);
        if (status == noErr && params.m_hasSyntheticBold) {
            // Force relayout for the bold pass
            ATSUClearLayoutCache(params.m_layout, 0);
            params.m_syntheticBoldPass = true;
            status = ATSUDrawText(params.m_layout, from, drawPortionLength, 0, 0);
            // Force relayout for the next pass
            ATSUClearLayoutCache(params.m_layout, 0);
            params.m_syntheticBoldPass = false;
        }
        CGContextTranslateCTM(context, -shadowSize.width(), -shadowSize.height());
        graphicsContext->setFillColor(fillColor);
    }

    status = ATSUDrawText(params.m_layout, from, drawPortionLength, 0, 0);
    if (status == noErr && params.m_hasSyntheticBold) {
        // Force relayout for the bold pass
        ATSUClearLayoutCache(params.m_layout, 0);
        params.m_syntheticBoldPass = true;
        status = ATSUDrawText(params.m_layout, from, drawPortionLength, 0, 0);
    }
    CGContextTranslateCTM(context, -point.x(), -point.y());

    if (status != noErr)
        // Nothing to do but report the error (dev build only).
        LOG_ERROR("ATSUDrawText() failed(%d)", status);

    if (hasSimpleShadow)
        graphicsContext->setShadow(shadowSize, shadowBlur, shadowColor);
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts) const
{
    if (run.length() == 0)
        return 0;

    ATSULayoutParameters params(run, fallbackFonts);
    params.initialize(this);
    
    OSStatus status;
    
    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    status = ATSUGetGlyphBounds(params.m_layout, 0, 0, 0, run.length(), kATSUseFractionalOrigins, 1, &firstGlyphBounds, &actualNumBounds);    
    if (status != noErr)
        LOG_ERROR("ATSUGetGlyphBounds() failed(%d)", status);
    if (actualNumBounds != 1)
        LOG_ERROR("unexpected result from ATSUGetGlyphBounds(): actualNumBounds(%d) != 1", actualNumBounds);

    return max(FixedToFloat(firstGlyphBounds.upperRight.x), FixedToFloat(firstGlyphBounds.lowerRight.x)) -
           min(FixedToFloat(firstGlyphBounds.upperLeft.x), FixedToFloat(firstGlyphBounds.lowerLeft.x));
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x, bool /*includePartialGlyphs*/) const
{
    OwnArrayPtr<UChar> charactersWithOverride;
    TextRun adjustedRun = copyRunForDirectionalOverrideIfNecessary(run, charactersWithOverride);

    ATSULayoutParameters params(adjustedRun);
    params.initialize(this);

    UniCharArrayOffset primaryOffset = 0;
    
    // FIXME: No idea how to avoid including partial glyphs.
    // Not even sure if that's the behavior this yields now.
    Boolean isLeading;
    UniCharArrayOffset secondaryOffset = 0;
    OSStatus status = ATSUPositionToOffset(params.m_layout, FloatToFixed(x), FloatToFixed(-1), &primaryOffset, &isLeading, &secondaryOffset);
    unsigned offset;
    if (status == noErr) {
        offset = (unsigned)primaryOffset;
        if (run.directionalOverride() && offset > 0)
            offset--;
    } else
        // Failed to find offset!  Return 0 offset.
        offset = 0;

    return offset;
}

}
#endif // USE(ATSUI)
