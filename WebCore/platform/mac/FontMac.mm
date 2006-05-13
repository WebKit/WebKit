/**
 * This file is part of the html renderer for KDE.
 *
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#import "config.h"
#import "Font.h"

#import "Logging.h"
#import "BlockExceptions.h"
#import "FoundationExtras.h"

#import "FontFallbackList.h"
#import "GraphicsContext.h"
#import "KWQKHTMLSettings.h"

#import "FontData.h"
#import "WebTextRendererFactory.h"

#import "IntRect.h"

#import "WebCoreSystemInterface.h"
#import "WebCoreTextRenderer.h"

#define SYNTHETIC_OBLIQUE_ANGLE 14

#define POP_DIRECTIONAL_FORMATTING 0x202C
#define LEFT_TO_RIGHT_OVERRIDE 0x202D
#define RIGHT_TO_LEFT_OVERRIDE 0x202E

using namespace std;

namespace WebCore {

FontFallbackList::FontFallbackList()
:m_pitch(UnknownPitch), m_font(nil)
{
    m_platformFont.font = nil;
}

FontFallbackList::~FontFallbackList()
{
    KWQRelease(m_platformFont.font);
}

const FontPlatformData& FontFallbackList::platformFont(const FontDescription& fontDescription) const
{
    if (!m_platformFont.font) {
        CREATE_FAMILY_ARRAY(fontDescription, families);
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        int traits = 0;
        if (fontDescription.italic())
            traits |= NSItalicFontMask;
        if (fontDescription.weight() >= WebCore::cBoldWeight)
            traits |= NSBoldFontMask;
        m_platformFont = [[WebTextRendererFactory sharedFactory] 
                                     fontWithFamilies:families traits:traits size:fontDescription.computedPixelSize()];
        KWQRetain(m_platformFont.font);
        m_platformFont.forPrinter = fontDescription.usePrinterFont();
        END_BLOCK_OBJC_EXCEPTIONS;
    }
    return m_platformFont;
}

void FontFallbackList::setPlatformFont(const FontPlatformData& platformData)
{
    m_platformFont = platformData;
    KWQRetain(m_platformFont.font);
}

FontData* FontFallbackList::primaryFont(const FontDescription& fontDescription) const
{
    if (!m_font)
        m_font = [[WebTextRendererFactory sharedFactory] rendererWithFont:platformFont(fontDescription)];
    return m_font;
}

void FontFallbackList::determinePitch(const FontDescription& fontDescription) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([[WebTextRendererFactory sharedFactory] isFontFixedPitch:platformFont(fontDescription)])
        m_pitch = FixedPitch;
    else
        m_pitch = VariablePitch;
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FontFallbackList::invalidate()
{
    m_font = 0;
    KWQRelease(m_platformFont.font);
    m_platformFont.font = nil;
    m_pitch = UnknownPitch;
}

// =================================================================
// Font Class (Platform-Specific Portion)
// =================================================================

struct ATSULayoutParameters
{
    ATSULayoutParameters(const TextRun& run, const TextStyle& style)
    :m_run(run), m_style(style),
     m_font(0), m_fonts(0), m_charBuffer(0), m_hasSyntheticBold(false), m_syntheticBoldPass(false), m_padPerSpace(0)
    {}

    void initialize(const Font* font);

    const TextRun& m_run;
    const TextStyle& m_style;
    
    const Font* m_font;
    
    ATSUTextLayout m_layout;
    const FontData **m_fonts;
    
    UChar *m_charBuffer;
    bool m_hasSyntheticBold;
    bool m_syntheticBoldPass;
    float m_padPerSpace;
};

// Be sure to free the array allocated by this function.
static TextRun addDirectionalOverride(const TextRun& run, bool rtl)
{
    UChar* charactersWithOverride = new UChar[run.length() + 2];
    charactersWithOverride[0] = rtl ? RIGHT_TO_LEFT_OVERRIDE : LEFT_TO_RIGHT_OVERRIDE;
    memcpy(&charactersWithOverride[1], run.data(0), sizeof(UChar) * run.length());
    charactersWithOverride[run.length() + 1] = POP_DIRECTIONAL_FORMATTING;

    return TextRun(charactersWithOverride, run.length() + 2, run.from() + 1, run.to() + 1);
}

static void initializeATSUStyle(const FontData* fontData)
{
    // The two NSFont calls in this method (pointSize and _atsFontID) do not raise exceptions.

    if (!fontData->m_ATSUStyleInitialized) {
        OSStatus status;
        ByteCount propTableSize;
        
        status = ATSUCreateStyle(&fontData->m_ATSUStyle);
        if (status != noErr)
            LOG_ERROR("ATSUCreateStyle failed (%d)", status);
    
        ATSUFontID fontID = wkGetNSFontATSUFontId(fontData->m_font.font);
        if (fontID == 0) {
            ATSUDisposeStyle(fontData->m_ATSUStyle);
            LOG_ERROR("unable to get ATSUFontID for %@", fontData->m_font.font);
            return;
        }
        
        CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);
        if (fontData->m_font.syntheticOblique)
            transform = CGAffineTransformConcat(transform, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
        Fixed fontSize = FloatToFixed([fontData->m_font.font pointSize]);
        // Turn off automatic kerning until it is supported in the CG code path (6136 in bugzilla)
        Fract kerningInhibitFactor = FloatToFract(1.0);
        ATSUAttributeTag styleTags[4] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag, kATSUKerningInhibitFactorTag };
        ByteCount styleSizes[4] = { sizeof(Fixed), sizeof(ATSUFontID), sizeof(CGAffineTransform), sizeof(Fract) };
        ATSUAttributeValuePtr styleValues[4] = { &fontSize, &fontID, &transform, &kerningInhibitFactor };
        status = ATSUSetAttributes(fontData->m_ATSUStyle, 4, styleTags, styleSizes, styleValues);
        if (status != noErr)
            LOG_ERROR("ATSUSetAttributes failed (%d)", status);
        status = ATSFontGetTable(fontID, 'prop', 0, 0, 0, &propTableSize);
        if (status == noErr)    // naively assume that if a 'prop' table exists then it contains mirroring info
            fontData->m_ATSUMirrors = true;
        else if (status == kATSInvalidFontTableAccess)
            fontData->m_ATSUMirrors = false;
        else
            LOG_ERROR("ATSFontGetTable failed (%d)", status);

        // Turn off ligatures such as 'fi' to match the CG code path's behavior, until bugzilla 6135 is fixed.
        // Don't be too aggressive: if the font doesn't contain 'a', then assume that any ligatures it contains are
        // in characters that always go through ATSUI, and therefore allow them. Geeza Pro is an example.
        // See bugzilla 5166.
        if ([[fontData->m_font.font coveredCharacterSet] characterIsMember:'a']) {
            ATSUFontFeatureType featureTypes[] = { kLigaturesType };
            ATSUFontFeatureSelector featureSelectors[] = { kCommonLigaturesOffSelector };
            status = ATSUSetFontFeatures(fontData->m_ATSUStyle, 1, featureTypes, featureSelectors);
        }

        fontData->m_ATSUStyleInitialized = true;
    }
}

static OSStatus overrideLayoutOperation(ATSULayoutOperationSelector iCurrentOperation, ATSULineRef iLineRef, UInt32 iRefCon,
                                        void *iOperationCallbackParameterPtr, ATSULayoutOperationCallbackStatus *oCallbackStatus)
{
    ATSULayoutParameters *params = (ATSULayoutParameters *)iRefCon;
    OSStatus status;
    ItemCount count;
    ATSLayoutRecord *layoutRecords;

    if (params->m_style.applyWordRounding()) {
        status = ATSUDirectGetLayoutDataArrayPtrFromLineRef(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, true, (void **)&layoutRecords, &count);
        if (status != noErr) {
            *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
            return status;
        }
        
        Fixed lastNativePos = 0;
        float lastAdjustedPos = 0;
        const UChar *characters = params->m_run.data(params->m_run.from());
        const FontData **renderers = params->m_fonts + params->m_run.from();
        const FontData *renderer;
        const FontData *lastRenderer = 0;
        UChar ch, nextCh;
        ByteCount offset = layoutRecords[0].originalOffset;
        nextCh = *(UChar *)(((char *)characters)+offset);
        bool shouldRound = false;
        bool syntheticBoldPass = params->m_syntheticBoldPass;
        Fixed syntheticBoldOffset = 0;
        ATSGlyphRef spaceGlyph = 0;
        bool hasExtraSpacing = params->m_font->letterSpacing() || params->m_font->wordSpacing() | params->m_style.padding();
        float padding = params->m_style.padding();
        // In the CoreGraphics code path, the rounding hack is applied in logical order.
        // Here it is applied in visual left-to-right order, which may be better.
        ItemCount lastRoundingChar = 0;
        ItemCount i;
        for (i = 1; i < count; i++) {
            bool isLastChar = i == count - 1;
            renderer = renderers[offset / 2];
            if (renderer != lastRenderer) {
                lastRenderer = renderer;
                // The CoreGraphics interpretation of NSFontAntialiasedIntegerAdvancementsRenderingMode seems
                // to be "round each glyph's width to the nearest integer". This is not the same as ATSUI
                // does in any of its device-metrics modes.
                shouldRound = [renderer->m_font.font renderingMode] == NSFontAntialiasedIntegerAdvancementsRenderingMode;
                if (syntheticBoldPass) {
                    syntheticBoldOffset = FloatToFixed(renderer->m_syntheticBoldOffset);
                    spaceGlyph = renderer->m_spaceGlyph;
                }
            }
            float width = FixedToFloat(layoutRecords[i].realPos - lastNativePos);
            lastNativePos = layoutRecords[i].realPos;
            if (shouldRound)
                width = roundf(width);
            width += renderer->m_syntheticBoldOffset;
            if (renderer->m_treatAsFixedPitch ? width == renderer->m_spaceWidth : (layoutRecords[i-1].flags & kATSGlyphInfoIsWhiteSpace))
                width = renderer->m_adjustedSpaceWidth;

            if (hasExtraSpacing) {
                if (width && params->m_font->letterSpacing())
                    width +=params->m_font->letterSpacing();
                if (Font::treatAsSpace(nextCh)) {
                    if (params->m_style.padding()) {
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

            ch = nextCh;
            offset = layoutRecords[i].originalOffset;
            // Use space for nextCh at the end of the loop so that we get inside the rounding hack code.
            // We won't actually round unless the other conditions are satisfied.
            nextCh = isLastChar ? ' ' : *(UChar *)(((char *)characters)+offset);

            if (Font::isRoundingHackCharacter(ch))
                width = ceilf(width);
            lastAdjustedPos = lastAdjustedPos + width;
            if (Font::isRoundingHackCharacter(nextCh)
                && (!isLastChar
                    || params->m_style.applyRunRounding()
                    || (params->m_run.to() < (int)params->m_run.length() && 
                        Font::isRoundingHackCharacter(characters[params->m_run.to() - params->m_run.from()])))) {
                if (params->m_style.ltr())
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
                    layoutRecords[i-1].glyphID = spaceGlyph;
            }
            layoutRecords[i].realPos = FloatToFixed(lastAdjustedPos);
        }
        
        status = ATSUDirectReleaseLayoutDataArrayPtr(iLineRef, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent, (void **)&layoutRecords);
    }
    *oCallbackStatus = kATSULayoutOperationCallbackStatusHandled;
    return noErr;
}

void ATSULayoutParameters::initialize(const Font* font)
{
    m_font = font;

    // FIXME: It is probably best to always allocate a buffer for RTL, since even if for this
    // fontData ATSUMirrors is true, for a substitute fontData it might be false.
    const FontData* fontData = font->primaryFont();
    m_fonts = new const FontData*[m_run.length()];
    m_charBuffer = (UChar*)((font->isSmallCaps() || (m_style.rtl() && !fontData->m_ATSUMirrors)) ? new UChar[m_run.length()] : 0);
    
    // The only Cocoa calls here are to NSGraphicsContext, which does not raise exceptions.

    ATSUTextLayout layout;
    OSStatus status;
    ATSULayoutOperationOverrideSpecifier overrideSpecifier;
    
    initializeATSUStyle(fontData);
    
    // FIXME: This is currently missing the following required features that the CoreGraphics code path has:
    // - \n, \t, and nonbreaking space render as a space.
    // - Other control characters do not render (other code path uses zero-width spaces).

    UniCharCount totalLength = m_run.length();
    UniCharArrayOffset runTo = m_run.to();
    UniCharArrayOffset runFrom = m_run.from();
    
    if (m_charBuffer)
        memcpy(m_charBuffer, m_run.characters(), totalLength * sizeof(UChar));

    UniCharCount runLength = runTo - runFrom;
    
    status = ATSUCreateTextLayoutWithTextPtr(
            (m_charBuffer ? m_charBuffer : m_run.characters()),
            runFrom,        // offset
            runLength,      // length
            totalLength,    // total length
            1,              // styleRunCount
            &runLength,     // length of style run
            &fontData->m_ATSUStyle, 
            &layout);
    if (status != noErr)
        LOG_ERROR("ATSUCreateTextLayoutWithTextPtr failed(%d)", status);
    m_layout = layout;
    ATSUSetTextLayoutRefCon(m_layout, (UInt32)this);

    CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;
    Boolean rtl = m_style.rtl();
    overrideSpecifier.operationSelector = kATSULayoutOperationPostLayoutAdjustment;
    overrideSpecifier.overrideUPP = overrideLayoutOperation;
    ATSUAttributeTag tags[] = { kATSUCGContextTag, kATSULineLayoutOptionsTag, kATSULineDirectionTag, kATSULayoutOperationOverrideTag };
    ByteCount sizes[] = { sizeof(CGContextRef), sizeof(ATSLineLayoutOptions), sizeof(Boolean), sizeof(ATSULayoutOperationOverrideSpecifier) };
    ATSUAttributeValuePtr values[] = { &cgContext, &lineLayoutOptions, &rtl, &overrideSpecifier };
    
    status = ATSUSetLayoutControls(layout, (m_style.applyWordRounding() ? 4 : 3), tags, sizes, values);
    if (status != noErr)
        LOG_ERROR("ATSUSetLayoutControls failed(%d)", status);

    status = ATSUSetTransientFontMatching(layout, YES);
    if (status != noErr)
        LOG_ERROR("ATSUSetTransientFontMatching failed(%d)", status);

    m_hasSyntheticBold = false;
    ATSUFontID ATSUSubstituteFont;
    UniCharArrayOffset substituteOffset = runFrom;
    UniCharCount substituteLength;
    UniCharArrayOffset lastOffset;
    const FontData* substituteFontData = 0;

    while (substituteOffset < runTo) {
        lastOffset = substituteOffset;
        status = ATSUMatchFontsToText(layout, substituteOffset, kATSUToTextEnd, &ATSUSubstituteFont, &substituteOffset, &substituteLength);
        if (status == kATSUFontsMatched || status == kATSUFontsNotMatched) {
            // FIXME: Should go through fallback list eventually.
            substituteFontData = fontData->findSubstituteFontData(m_run.characters() + substituteOffset, substituteLength, m_font->fontDescription());
            if (substituteFontData) {
                initializeATSUStyle(substituteFontData);
                if (substituteFontData->m_ATSUStyle)
                    ATSUSetRunStyle(layout, substituteFontData->m_ATSUStyle, substituteOffset, substituteLength);
            } else
                substituteFontData = fontData;
        } else {
            substituteOffset = runTo;
            substituteLength = 0;
        }

        bool isSmallCap = false;
        UniCharArrayOffset firstSmallCap = 0;
        const FontData *r = fontData;
        UniCharArrayOffset i;
        for (i = lastOffset;  ; i++) {
            if (i == substituteOffset || i == substituteOffset + substituteLength) {
                if (isSmallCap) {
                    isSmallCap = false;
                    initializeATSUStyle(r->smallCapsFontData());
                    ATSUSetRunStyle(layout, r->smallCapsFontData()->m_ATSUStyle, firstSmallCap, i - firstSmallCap);
                }
                if (i == substituteOffset && substituteLength > 0)
                    r = substituteFontData;
                else
                    break;
            }
            if (m_style.rtl() && m_charBuffer && !r->m_ATSUMirrors)
                m_charBuffer[i] = u_charMirror(m_charBuffer[i]);
            if (m_font->isSmallCaps()) {
                UChar c = m_charBuffer[i];
                UChar newC;
                if (U_GET_GC_MASK(c) & U_GC_M_MASK)
                    m_fonts[i] = isSmallCap ? r->smallCapsFontData() : r;
                else if (!u_isUUppercase(c) && (newC = u_toupper(c)) != c) {
                    m_charBuffer[i] = newC;
                    if (!isSmallCap) {
                        isSmallCap = true;
                        firstSmallCap = i;
                    }
                    m_fonts[i] = r->smallCapsFontData();
                } else {
                    if (isSmallCap) {
                        isSmallCap = false;
                        initializeATSUStyle(r->smallCapsFontData());
                        ATSUSetRunStyle(layout, r->smallCapsFontData()->m_ATSUStyle, firstSmallCap, i - firstSmallCap);
                    }
                    m_fonts[i] = r;
                }
            } else
                m_fonts[i] = r;
            if (m_fonts[i]->m_syntheticBoldOffset)
                m_hasSyntheticBold = true;
        }
        substituteOffset += substituteLength;
    }
    if (m_style.padding()) {
        float numSpaces = 0;
        unsigned k;
        for (k = 0; k < totalLength; k++)
            if (Font::treatAsSpace(m_run[k]))
                numSpaces++;

        if (numSpaces == 0)
            m_padPerSpace = 0;
        else
            m_padPerSpace = ceilf(m_style.padding() / numSpaces);
    } else
        m_padPerSpace = 0;
}

static void disposeATSULayoutParameters(ATSULayoutParameters *params)
{
    ATSUDisposeTextLayout(params->m_layout);
    delete []params->m_charBuffer;
    delete []params->m_fonts;
}

Font::Font(const FontPlatformData& fontData)
:m_letterSpacing(0), m_wordSpacing(0)
{
    m_fontList = new FontFallbackList();
    m_fontList->setPlatformFont(fontData);
}

const FontPlatformData& Font::platformFont() const
{
    return m_fontList->platformFont(fontDescription());
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h) const
{
    int from = run.from();
    int to = run.to();
        
    TextRun completeRun(run);
    completeRun.makeComplete();
    
    TextRun adjustedRun = style.directionalOverride() ? addDirectionalOverride(completeRun, style.rtl()) : completeRun;
    if (style.directionalOverride()) {
        from++;
        to++;
    }

    ATSULayoutParameters params(adjustedRun, style);
    params.initialize(this);

    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    
    OSStatus status = ATSUGetGlyphBounds(params.m_layout, 0, 0, from, to - from, kATSUseFractionalOrigins, 1, &firstGlyphBounds, &actualNumBounds);
    if (status != noErr || actualNumBounds != 1) {
        static ATSTrapezoid zeroTrapezoid = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
        firstGlyphBounds = zeroTrapezoid;
    }
    disposeATSULayoutParameters(&params);
    
    float beforeWidth = MIN(FixedToFloat(firstGlyphBounds.lowerLeft.x), FixedToFloat(firstGlyphBounds.upperLeft.x));
    float afterWidth = MAX(FixedToFloat(firstGlyphBounds.lowerRight.x), FixedToFloat(firstGlyphBounds.upperRight.x));
    
    FloatRect rect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);

    if (style.directionalOverride())
        delete []adjustedRun.characters();

    return rect;
}

void Font::drawComplexText(GraphicsContext* graphicsContext, const TextRun& run, const TextStyle& style, const FloatPoint& point) const
{
    OSStatus status;
    
    int runLength = run.length();

    TextRun adjustedRun = style.directionalOverride() ? addDirectionalOverride(run, style.rtl()) : run;
    ATSULayoutParameters params(adjustedRun, style);
    params.initialize(this);

    [nsColor(graphicsContext->pen().color()) set];

    // ATSUI can't draw beyond -32768 to +32767 so we translate the CTM and tell ATSUI to draw at (0, 0).
    // FIXME: Cut the dependency on currentContext here.
    NSGraphicsContext *gContext = [NSGraphicsContext currentContext];
    CGContextRef context = (CGContextRef)[gContext graphicsPort];
    CGContextTranslateCTM(context, point.x(), point.y());
    bool flipped = [gContext isFlipped];
    if (!flipped)
        CGContextScaleCTM(context, 1.0, -1.0);
    status = ATSUDrawText(params.m_layout, adjustedRun.from(), runLength, 0, 0);
    if (status == noErr && params.m_hasSyntheticBold) {
        // Force relayout for the bold pass
        ATSUClearLayoutCache(params.m_layout, 0);
        params.m_syntheticBoldPass = true;
        status = ATSUDrawText(params.m_layout, adjustedRun.from(), runLength, 0, 0);
    }
    if (!flipped)
        CGContextScaleCTM(context, 1.0, -1.0);
    CGContextTranslateCTM(context, -point.x(), -point.y());

    if (status != noErr) {
        // Nothing to do but report the error (dev build only).
        LOG_ERROR("ATSUDrawText() failed(%d)", status);
    }

    disposeATSULayoutParameters(&params);
    
    if (style.directionalOverride())
        delete []adjustedRun.characters();
}

float Font::floatWidthForComplexText(const TextRun& run, const TextStyle& style) const
{
    ATSULayoutParameters params(run, style);
    params.initialize(this);
    
    OSStatus status;
    
    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    status = ATSUGetGlyphBounds(params.m_layout, 0, 0, run.from(), run.to() - run.from(), kATSUseFractionalOrigins, 1, &firstGlyphBounds, &actualNumBounds);    
    if (status != noErr)
        LOG_ERROR("ATSUGetGlyphBounds() failed(%d)", status);
    if (actualNumBounds != 1)
        LOG_ERROR("unexpected result from ATSUGetGlyphBounds(): actualNumBounds(%d) != 1", actualNumBounds);

    disposeATSULayoutParameters(&params);

    return MAX(FixedToFloat(firstGlyphBounds.upperRight.x), FixedToFloat(firstGlyphBounds.lowerRight.x)) -
           MIN(FixedToFloat(firstGlyphBounds.upperLeft.x), FixedToFloat(firstGlyphBounds.lowerLeft.x));
}

int Font::offsetForPositionForComplexText(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    TextRun adjustedRun = style.directionalOverride() ? addDirectionalOverride(run, style.rtl()) : run;
    
    ATSULayoutParameters params(adjustedRun, style);
    params.initialize(this);

    UniCharArrayOffset primaryOffset = adjustedRun.from();
    
    // FIXME: No idea how to avoid including partial glyphs.
    // Not even sure if that's the behavior this yields now.
    Boolean isLeading;
    UniCharArrayOffset secondaryOffset = 0;
    OSStatus status = ATSUPositionToOffset(params.m_layout, FloatToFixed(x), FloatToFixed(-1), &primaryOffset, &isLeading, &secondaryOffset);
    unsigned offset;
    if (status == noErr)
        offset = (unsigned)primaryOffset;
    else
        // Failed to find offset!  Return 0 offset.
        offset = 0;

    disposeATSULayoutParameters(&params);
    
    if (style.directionalOverride())
        delete []adjustedRun.characters();

    return offset - adjustedRun.from();
}

void Font::drawGlyphs(GraphicsContext* context, const FontData* font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    // FIXME: Grab the CGContext from the GraphicsContext eventually, when we have made sure to shield against flipping caused by calls into us
    // from Safari.
    NSGraphicsContext *gContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext = (CGContextRef)[gContext graphicsPort];

    bool originalShouldUseFontSmoothing = wkCGContextGetShouldSmoothFonts(cgContext);
    CGContextSetShouldSmoothFonts(cgContext, WebCoreShouldUseFontSmoothing());
    
    const FontPlatformData& platformData = font->platformData();
    NSFont* drawFont;
    if ([gContext isDrawingToScreen]) {
        drawFont = [platformData.font screenFont];
        if (drawFont != platformData.font)
            // We are getting this in too many places (3406411); use ERROR so it only prints on debug versions for now. (We should debug this also, eventually).
            LOG_ERROR("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.",
                [[[platformData.font fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    } else {
        drawFont = [platformData.font printerFont];
        if (drawFont != platformData.font)
            NSLog(@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.",
                [[[platformData.font fontDescriptor] fontAttributes] objectForKey:NSFontNameAttribute]);
    }
    
    CGContextSetFont(cgContext, wkGetCGFontFromNSFont(drawFont));

    CGAffineTransform matrix;
    memcpy(&matrix, [drawFont matrix], sizeof(matrix));
    if ([gContext isFlipped]) {
        matrix.b = -matrix.b;
        matrix.d = -matrix.d;
    }
    if (platformData.syntheticOblique)
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, -tanf(SYNTHETIC_OBLIQUE_ANGLE * acosf(0) / 90), 1, 0, 0)); 
    CGContextSetTextMatrix(cgContext, matrix);

    wkSetCGFontRenderingMode(cgContext, drawFont);
    CGContextSetFontSize(cgContext, 1.0f);

    [nsColor(context->pen().color()) set];

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    if (font->m_syntheticBoldOffset) {
        CGContextSetTextPosition(cgContext, point.x() + font->m_syntheticBoldOffset, point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), glyphBuffer.advances(from), numGlyphs);
    }

    CGContextSetShouldSmoothFonts(cgContext, originalShouldUseFontSmoothing);
}

}
