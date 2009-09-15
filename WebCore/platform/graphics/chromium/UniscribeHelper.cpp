/*
 * Copyright (c) 2006, 2007, 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UniscribeHelper.h"

#include <windows.h>

#include "FontUtilsChromiumWin.h"
#include "PlatformContextSkia.h"
#include "SkiaFontWin.h"
#include "SkPoint.h"
#include <wtf/Assertions.h>

namespace WebCore {

// This function is used to see where word spacing should be applied inside
// runs. Note that this must match Font::treatAsSpace so we all agree where
// and how much space this is, so we don't want to do more general Unicode
// "is this a word break" thing.
static bool treatAsSpace(UChar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0;
}

// SCRIPT_FONTPROPERTIES contains glyph indices for default, invalid
// and blank glyphs. Just because ScriptShape succeeds does not mean
// that a text run is rendered correctly. Some characters may be rendered
// with default/invalid/blank glyphs. Therefore, we need to check if the glyph
// array returned by ScriptShape contains any of those glyphs to make
// sure that the text run is rendered successfully.
static bool containsMissingGlyphs(WORD *glyphs,
                                  int length,
                                  SCRIPT_FONTPROPERTIES* properties)
{
    for (int i = 0; i < length; ++i) {
        if (glyphs[i] == properties->wgDefault
            || (glyphs[i] == properties->wgInvalid
            && glyphs[i] != properties->wgBlank))
            return true;
    }

    return false;
}

// HFONT is the 'incarnation' of 'everything' about font, but it's an opaque
// handle and we can't directly query it to make a new HFONT sharing
// its characteristics (height, style, etc) except for family name.
// This function uses GetObject to convert HFONT back to LOGFONT,
// resets the fields of LOGFONT and calculates style to use later
// for the creation of a font identical to HFONT other than family name.
static void setLogFontAndStyle(HFONT hfont, LOGFONT *logfont, int *style)
{
    ASSERT(hfont && logfont);
    if (!hfont || !logfont)
        return;

    GetObject(hfont, sizeof(LOGFONT), logfont);
    // We reset these fields to values appropriate for CreateFontIndirect.
    // while keeping lfHeight, which is the most important value in creating
    // a new font similar to hfont.
    logfont->lfWidth = 0;
    logfont->lfEscapement = 0;
    logfont->lfOrientation = 0;
    logfont->lfCharSet = DEFAULT_CHARSET;
    logfont->lfOutPrecision = OUT_TT_ONLY_PRECIS;
    logfont->lfQuality = DEFAULT_QUALITY;  // Honor user's desktop settings.
    logfont->lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    if (style)
        *style = getStyleFromLogfont(logfont);
}

UniscribeHelper::UniscribeHelper(const UChar* input,
                                int inputLength,
                                bool isRtl,
                                HFONT hfont,
                                SCRIPT_CACHE* scriptCache,
                                SCRIPT_FONTPROPERTIES* fontProperties)
    : m_input(input)
    , m_inputLength(inputLength)
    , m_isRtl(isRtl)
    , m_hfont(hfont)
    , m_scriptCache(scriptCache)
    , m_fontProperties(fontProperties)
    , m_directionalOverride(false)
    , m_inhibitLigate(false)
    , m_letterSpacing(0)
    , m_spaceWidth(0)
    , m_wordSpacing(0)
    , m_ascent(0)
    , m_disableFontFallback(false)

{
    m_logfont.lfFaceName[0] = 0;
}

UniscribeHelper::~UniscribeHelper()
{
}

void UniscribeHelper::initWithOptionalLengthProtection(bool lengthProtection)
{
    // We cap the input length and just don't do anything. We'll allocate a lot
    // of things of the size of the number of characters, so the allocated
    // memory will be several times the input length. Plus shaping such a large
    // buffer may be a form of denial of service. No legitimate text should be
    // this long.  It also appears that Uniscribe flatly rejects very long
    // strings, so we don't lose anything by doing this.
    //
    // The input length protection may be disabled by the unit tests to cause
    // an error condition.
    static const int kMaxInputLength = 65535;
    if (m_inputLength == 0 || (lengthProtection && m_inputLength > kMaxInputLength))
        return;

    fillRuns();
    fillShapes();
    fillScreenOrder();
}

int UniscribeHelper::width() const
{
    int width = 0;
    for (int itemIndex = 0; itemIndex < static_cast<int>(m_runs.size()); itemIndex++)
        width += advanceForItem(itemIndex);
    return width;
}

void UniscribeHelper::justify(int additionalSpace)
{
    // Count the total number of glyphs we have so we know how big to make the
    // buffers below.
    int totalGlyphs = 0;
    for (size_t run = 0; run < m_runs.size(); run++) {
        int runIndex = m_screenOrder[run];
        totalGlyphs += static_cast<int>(m_shapes[runIndex].glyphLength());
    }
    if (totalGlyphs == 0)
        return;  // Nothing to do.

    // We make one big buffer in screen order of all the glyphs we are drawing
    // across runs so that the justification function will adjust evenly across
    // all glyphs.
    Vector<SCRIPT_VISATTR, 64> visualAttributes;
    visualAttributes.resize(totalGlyphs);
    Vector<int, 64> advances;
    advances.resize(totalGlyphs);
    Vector<int, 64> justify;
    justify.resize(totalGlyphs);

    // Build the packed input.
    int destIndex = 0;
    for (size_t run = 0; run < m_runs.size(); run++) {
        int runIndex = m_screenOrder[run];
        const Shaping& shaping = m_shapes[runIndex];

        for (int i = 0; i < shaping.glyphLength(); i++, destIndex++) {
            memcpy(&visualAttributes[destIndex], &shaping.m_visualAttributes[i],
                   sizeof(SCRIPT_VISATTR));
            advances[destIndex] = shaping.m_advance[i];
        }
    }

    // The documentation for Scriptjustify is wrong, the parameter is the space
    // to add and not the width of the column you want.
    const int minKashida = 1;  // How do we decide what this should be?
    ScriptJustify(&visualAttributes[0], &advances[0], totalGlyphs,
                  additionalSpace, minKashida, &justify[0]);

    // Now we have to unpack the justification amounts back into the runs so
    // the glyph indices match.
    int globalGlyphIndex = 0;
    for (size_t run = 0; run < m_runs.size(); run++) {
        int runIndex = m_screenOrder[run];
        Shaping& shaping = m_shapes[runIndex];

        shaping.m_justify.resize(shaping.glyphLength());
        for (int i = 0; i < shaping.glyphLength(); i++, globalGlyphIndex++)
            shaping.m_justify[i] = justify[globalGlyphIndex];
    }
}

int UniscribeHelper::characterToX(int offset) const
{
    HRESULT hr;
    ASSERT(offset <= m_inputLength);

    // Our algorithm is to traverse the items in screen order from left to
    // right, adding in each item's screen width until we find the item with
    // the requested character in it.
    int width = 0;
    for (size_t screenIndex = 0; screenIndex < m_runs.size(); screenIndex++) {
        // Compute the length of this run.
        int itemIndex = m_screenOrder[screenIndex];
        const SCRIPT_ITEM& item = m_runs[itemIndex];
        const Shaping& shaping = m_shapes[itemIndex];
        int itemLength = shaping.charLength();

        if (offset >= item.iCharPos && offset <= item.iCharPos + itemLength) {
            // Character offset is in this run.
            int charLength = offset - item.iCharPos;

            int curX = 0;
            hr = ScriptCPtoX(charLength, FALSE, itemLength,
                             shaping.glyphLength(),
                             &shaping.m_logs[0], &shaping.m_visualAttributes[0],
                             shaping.effectiveAdvances(), &item.a, &curX);
            if (FAILED(hr))
                return 0;

            width += curX + shaping.m_prePadding;
            ASSERT(width >= 0);
            return width;
        }

        // Move to the next item.
        width += advanceForItem(itemIndex);
    }
    ASSERT(width >= 0);
    return width;
}

int UniscribeHelper::xToCharacter(int x) const
{
    // We iterate in screen order until we find the item with the given pixel
    // position in it. When we find that guy, we ask Uniscribe for the
    // character index.
    HRESULT hr;
    for (size_t screenIndex = 0; screenIndex < m_runs.size(); screenIndex++) {
        int itemIndex = m_screenOrder[screenIndex];
        int itemAdvance = advanceForItem(itemIndex);

        // Note that the run may be empty if shaping failed, so we want to skip
        // over it.
        const Shaping& shaping = m_shapes[itemIndex];
        int itemLength = shaping.charLength();
        if (x <= itemAdvance && itemLength > 0) {
            // The requested offset is within this item.
            const SCRIPT_ITEM& item = m_runs[itemIndex];

            // Account for the leading space we've added to this run that
            // Uniscribe doesn't know about.
            x -= shaping.m_prePadding;

            int charX = 0;
            int trailing;
            hr = ScriptXtoCP(x, itemLength, shaping.glyphLength(),
                             &shaping.m_logs[0], &shaping.m_visualAttributes[0],
                             shaping.effectiveAdvances(), &item.a, &charX,
                             &trailing);

            // The character offset is within the item. We need to add the
            // item's offset to transform it into the space of the TextRun
            return charX + item.iCharPos;
        }

        // The offset is beyond this item, account for its length and move on.
        x -= itemAdvance;
    }

    // Error condition, we don't know what to do if we don't have that X
    // position in any of our items.
    return 0;
}

void UniscribeHelper::draw(GraphicsContext* graphicsContext,
                           HDC dc, int x, int y, int from, int to)
{
    HGDIOBJ oldFont = 0;
    int curX = x;
    bool firstRun = true;
    bool useWindowsDrawing = windowsCanHandleTextDrawing(graphicsContext);

    for (size_t screenIndex = 0; screenIndex < m_runs.size(); screenIndex++) {
        int itemIndex = m_screenOrder[screenIndex];
        const SCRIPT_ITEM& item = m_runs[itemIndex];
        const Shaping& shaping = m_shapes[itemIndex];

        // Character offsets within this run. THESE MAY NOT BE IN RANGE and may
        // be negative, etc. The code below handles this.
        int fromChar = from - item.iCharPos;
        int toChar = to - item.iCharPos;

        // See if we need to draw any characters in this item.
        if (shaping.charLength() == 0 ||
            fromChar >= shaping.charLength() || toChar <= 0) {
            // No chars in this item to display.
            curX += advanceForItem(itemIndex);
            continue;
        }

        // Compute the starting glyph within this span. |from| and |to| are
        // global offsets that may intersect arbitrarily with our local run.
        int fromGlyph, afterGlyph;
        if (item.a.fRTL) {
            // To compute the first glyph when going RTL, we use |to|.
            if (toChar >= shaping.charLength())
                // The end of the text is after (to the left) of us.
                fromGlyph = 0;
            else {
                // Since |to| is exclusive, the first character we draw on the
                // left is actually the one right before (to the right) of
                // |to|.
                fromGlyph = shaping.m_logs[toChar - 1];
            }

            // The last glyph is actually the first character in the range.
            if (fromChar <= 0) {
                // The first character to draw is before (to the right) of this
                // span, so draw all the way to the end.
                afterGlyph = shaping.glyphLength();
            } else {
                // We want to draw everything up until the character to the
                // right of |from|. To the right is - 1, so we look that up
                // (remember our character could be more than one glyph, so we
                // can't look up our glyph and add one).
                afterGlyph = shaping.m_logs[fromChar - 1];
            }
        } else {
            // Easy case, everybody agrees about directions. We only need to
            // handle boundary conditions to get a range inclusive at the
            // beginning, and exclusive at the ending. We have to do some
            // computation to see the glyph one past the end.
            fromGlyph = shaping.m_logs[fromChar < 0 ? 0 : fromChar];
            if (toChar >= shaping.charLength())
                afterGlyph = shaping.glyphLength();
            else
                afterGlyph = shaping.m_logs[toChar];
        }

        // Account for the characters that were skipped in this run. When
        // WebKit asks us to draw a subset of the run, it actually tells us
        // to draw at the X offset of the beginning of the run, since it
        // doesn't know the internal position of any of our characters.
        const int* effectiveAdvances = shaping.effectiveAdvances();
        int innerOffset = 0;
        for (int i = 0; i < fromGlyph; i++)
            innerOffset += effectiveAdvances[i];

        // Actually draw the glyphs we found.
        int glyphCount = afterGlyph - fromGlyph;
        if (fromGlyph >= 0 && glyphCount > 0) {
            // Account for the preceding space we need to add to this run. We
            // don't need to count for the following space because that will be
            // counted in advanceForItem below when we move to the next run.
            innerOffset += shaping.m_prePadding;

            // Pass 0 in when there is no justification.
            const int* justify = shaping.m_justify.size() == 0 ? 0 : &shaping.m_justify[fromGlyph];

            if (useWindowsDrawing) {
                if (firstRun) {
                    oldFont = SelectObject(dc, shaping.m_hfont);
                    firstRun = false;
                } else
                    SelectObject(dc, shaping.m_hfont);
            }

            // Fonts with different ascents can be used to render different
            // runs.  'Across-runs' y-coordinate correction needs to be
            // adjusted for each font.
            bool textOutOk = false;
            for (int executions = 0; executions < 2; ++executions) {
                if (useWindowsDrawing) {
                    HRESULT hr = ScriptTextOut(dc, shaping.m_scriptCache,
                                               curX + innerOffset,
                                               y - shaping.m_ascentOffset,
                                               0, 0, &item.a, 0, 0,
                                               &shaping.m_glyphs[fromGlyph],
                                               glyphCount,
                                               &shaping.m_advance[fromGlyph],
                                               justify,
                                               &shaping.m_offsets[fromGlyph]);
                    ASSERT(S_OK == hr);
                    textOutOk = (hr == S_OK);
                } else {
                    SkPoint origin;
                    origin.fX = curX + + innerOffset;
                    origin.fY = y + m_ascent;
                    textOutOk = paintSkiaText(graphicsContext,
                                              shaping.m_hfont,
                                              glyphCount,
                                              &shaping.m_glyphs[fromGlyph],
                                              &shaping.m_advance[fromGlyph],
                                              &shaping.m_offsets[fromGlyph],
                                              &origin);
                }

                if (!textOutOk && 0 == executions) {
                    // If TextOut is called from the renderer it might fail
                    // because the sandbox is preventing it from opening the
                    // font files.  If we are running in the renderer,
                    // TryToPreloadFont is overridden to ask the browser to
                    // preload the font for us so we can access it.
                    tryToPreloadFont(shaping.m_hfont);
                    continue;
                }
                break;
            }
        }

        curX += advanceForItem(itemIndex);
    }

    if (oldFont)
        SelectObject(dc, oldFont);
}

WORD UniscribeHelper::firstGlyphForCharacter(int charOffset) const
{
    // Find the run for the given character.
    for (int i = 0; i < static_cast<int>(m_runs.size()); i++) {
        int firstChar = m_runs[i].iCharPos;
        const Shaping& shaping = m_shapes[i];
        int localOffset = charOffset - firstChar;
        if (localOffset >= 0 && localOffset < shaping.charLength()) {
            // The character is in this run, return the first glyph for it
            // (should generally be the only glyph). It seems Uniscribe gives
            // glyph 0 for empty, which is what we want to return in the
            // "missing" case.
            size_t glyphIndex = shaping.m_logs[localOffset];
            if (glyphIndex >= shaping.m_glyphs.size()) {
                // The glyph should be in this run, but the run has too few
                // actual characters. This can happen when shaping the run
                // fails, in which case, we should have no data in the logs at
                // all.
                ASSERT(shaping.m_glyphs.size() == 0);
                return 0;
            }
            return shaping.m_glyphs[glyphIndex];
        }
    }

    return 0;
}

void UniscribeHelper::fillRuns()
{
    HRESULT hr;
    m_runs.resize(UNISCRIBE_HELPER_STACK_RUNS);

    SCRIPT_STATE inputState;
    inputState.uBidiLevel = m_isRtl;
    inputState.fOverrideDirection = m_directionalOverride;
    inputState.fInhibitSymSwap = false;
    inputState.fCharShape = false;  // Not implemented in Uniscribe
    inputState.fDigitSubstitute = false;  // Do we want this for Arabic?
    inputState.fInhibitLigate = m_inhibitLigate;
    inputState.fDisplayZWG = false;  // Don't draw control characters.
    inputState.fArabicNumContext = m_isRtl;  // Do we want this for Arabic?
    inputState.fGcpClusters = false;
    inputState.fReserved = 0;
    inputState.fEngineReserved = 0;
    // The psControl argument to ScriptItemize should be non-0 for RTL text,
    // per http://msdn.microsoft.com/en-us/library/ms776532.aspx . So use a
    // SCRIPT_CONTROL that is set to all zeros.  Zero as a locale ID means the
    // neutral locale per http://msdn.microsoft.com/en-us/library/ms776294.aspx
    static SCRIPT_CONTROL inputControl = {0, // uDefaultLanguage    :16;
                                           0, // fContextDigits      :1;
                                           0, // fInvertPreBoundDir  :1;
                                           0, // fInvertPostBoundDir :1;
                                           0, // fLinkStringBefore   :1;
                                           0, // fLinkStringAfter    :1;
                                           0, // fNeutralOverride    :1;
                                           0, // fNumericOverride    :1;
                                           0, // fLegacyBidiClass    :1;
                                           0, // fMergeNeutralItems  :1;
                                           0};// fReserved           :7;
    // Calling ScriptApplyDigitSubstitution( 0, &inputControl, &inputState)
    // here would be appropriate if we wanted to set the language ID, and get
    // local digit substitution behavior.  For now, don't do it.

    while (true) {
        int numberOfItems = 0;

        // Ideally, we would have a way to know the runs before and after this
        // one, and put them into the control parameter of ScriptItemize. This
        // would allow us to shape characters properly that cross style
        // boundaries (WebKit bug 6148).
        //
        // We tell ScriptItemize that the output list of items is one smaller
        // than it actually is. According to Mozilla bug 366643, if there is
        // not enough room in the array on pre-SP2 systems, ScriptItemize will
        // write one past the end of the buffer.
        //
        // ScriptItemize is very strange. It will often require a much larger
        // ITEM buffer internally than it will give us as output. For example,
        // it will say a 16-item buffer is not big enough, and will write
        // interesting numbers into all those items. But when we give it a 32
        // item buffer and it succeeds, it only has one item output.
        //
        // It seems to be doing at least two passes, the first where it puts a
        // lot of intermediate data into our items, and the second where it
        // collates them.
        hr = ScriptItemize(m_input, m_inputLength,
                           static_cast<int>(m_runs.size()) - 1, &inputControl,
                           &inputState,
                           &m_runs[0], &numberOfItems);
        if (SUCCEEDED(hr)) {
            m_runs.resize(numberOfItems);
            break;
        }
        if (hr != E_OUTOFMEMORY) {
            // Some kind of unexpected error.
            m_runs.resize(0);
            break;
        }
        // There was not enough items for it to write into, expand.
        m_runs.resize(m_runs.size() * 2);
    }
}

bool UniscribeHelper::shape(const UChar* input,
                            int itemLength,
                            int numGlyphs,
                            SCRIPT_ITEM& run,
                            Shaping& shaping)
{
    HFONT hfont = m_hfont;
    SCRIPT_CACHE* scriptCache = m_scriptCache;
    SCRIPT_FONTPROPERTIES* fontProperties = m_fontProperties;
    int ascent = m_ascent;
    HDC tempDC = 0;
    HGDIOBJ oldFont = 0;
    HRESULT hr;
    // When used to fill up glyph pages for simple scripts in non-BMP,
    // we don't want any font fallback in this class. The simple script
    // font path can take care of font fallback.
    bool lastFallbackTried = m_disableFontFallback;
    bool result;

    int generatedGlyphs = 0;

    // In case HFONT passed in ctor cannot render this run, we have to scan
    // other fonts from the beginning of the font list.
    resetFontIndex();

    // Compute shapes.
    while (true) {
        shaping.m_logs.resize(itemLength);
        shaping.m_glyphs.resize(numGlyphs);
        shaping.m_visualAttributes.resize(numGlyphs);

#ifdef PURIFY
        // http://code.google.com/p/chromium/issues/detail?id=5309
        // Purify isn't able to track the assignments that ScriptShape makes to
        // shaping.m_glyphs. Consequently, any bytes with value 0xCD that it
        // writes, will be considered un-initialized data.
        //
        // This hack avoid the false-positive UMRs by marking the buffer as
        // initialized.
        //
        // FIXME: A better solution would be to use Purify's API and mark only
        // the populated range as initialized:
        //
        //     PurifyMarkAsInitialized(
        //         &shaping.m_glyphs[0],
        //         sizeof(shaping.m_glyphs[0] * generatedGlyphs);

        ZeroMemory(&shaping.m_glyphs[0],
                   sizeof(shaping.m_glyphs[0]) * shaping.m_glyphs.size());
#endif

        // Firefox sets SCRIPT_ANALYSIS.SCRIPT_STATE.fDisplayZWG to true
        // here. Is that what we want? It will display control characters.
        hr = ScriptShape(tempDC, scriptCache, input, itemLength,
                         numGlyphs, &run.a,
                         &shaping.m_glyphs[0], &shaping.m_logs[0],
                         &shaping.m_visualAttributes[0], &generatedGlyphs);
        if (hr == E_PENDING) {
            // Allocate the DC.
            tempDC = GetDC(0);
            oldFont = SelectObject(tempDC, hfont);
            continue;
        } else if (hr == E_OUTOFMEMORY) {
            numGlyphs *= 2;
            continue;
        } else if (SUCCEEDED(hr) && (lastFallbackTried || !containsMissingGlyphs(&shaping.m_glyphs[0], generatedGlyphs, fontProperties)))
            break;

        // The current font can't render this run. clear DC and try
        // next font.
        if (tempDC) {
            SelectObject(tempDC, oldFont);
            ReleaseDC(0, tempDC);
            tempDC = 0;
        }

        if (!m_disableFontFallback &&
            nextWinFontData(&hfont, &scriptCache, &fontProperties, &ascent)) {
            // The primary font does not support this run. Try next font.
            // In case of web page rendering, they come from fonts specified in
            // CSS stylesheets.
            continue;
        } else if (!lastFallbackTried) {
            lastFallbackTried = true;

            // Generate a last fallback font based on the script of
            // a character to draw while inheriting size and styles
            // from the primary font
            if (!m_logfont.lfFaceName[0])
                setLogFontAndStyle(m_hfont, &m_logfont, &m_style);

            // TODO(jungshik): generic type should come from webkit for
            // UniscribeHelperTextRun (a derived class used in webkit).
            const UChar *family = getFallbackFamily(input, itemLength,
                FontDescription::StandardFamily, 0, 0);
            bool fontOk = getDerivedFontData(family, m_style, &m_logfont,
                                              &ascent, &hfont, &scriptCache);

            if (!fontOk) {
                // If this GetDerivedFontData is called from the renderer it
                // might fail because the sandbox is preventing it from opening
                // the font files.  If we are running in the renderer,
                // TryToPreloadFont is overridden to ask the browser to preload
                // the font for us so we can access it.
                tryToPreloadFont(hfont);

                // Try again.
                fontOk = getDerivedFontData(family, m_style, &m_logfont,
                                             &ascent, &hfont, &scriptCache);
                ASSERT(fontOk);
            }

            // TODO(jungshik) : Currently GetDerivedHFont always returns a
            // a valid HFONT, but in the future, I may change it to return 0.
            ASSERT(hfont);

            // We don't need a font_properties for the last resort fallback font
            // because we don't have anything more to try and are forced to
            // accept empty glyph boxes. If we tried a series of fonts as
            // 'last-resort fallback', we'd need it, but currently, we don't.
            continue;
        } else if (hr == USP_E_SCRIPT_NOT_IN_FONT) {
            run.a.eScript = SCRIPT_UNDEFINED;
            continue;
        } else if (FAILED(hr)) {
            // Error shaping.
            generatedGlyphs = 0;
            result = false;
            goto cleanup;
        }
    }

    // Sets Windows font data for this run to those corresponding to
    // a font supporting this run. we don't need to store font_properties
    // because it's not used elsewhere.
    shaping.m_hfont = hfont;
    shaping.m_scriptCache = scriptCache;

    // The ascent of a font for this run can be different from
    // that of the primary font so that we need to keep track of
    // the difference per run and take that into account when calling
    // ScriptTextOut in |draw|. Otherwise, different runs rendered by
    // different fonts would not be aligned vertically.
    shaping.m_ascentOffset = m_ascent ? ascent - m_ascent : 0;
    result = true;

  cleanup:
    shaping.m_glyphs.resize(generatedGlyphs);
    shaping.m_visualAttributes.resize(generatedGlyphs);
    shaping.m_advance.resize(generatedGlyphs);
    shaping.m_offsets.resize(generatedGlyphs);
    if (tempDC) {
        SelectObject(tempDC, oldFont);
        ReleaseDC(0, tempDC);
    }
    // On failure, our logs don't mean anything, so zero those out.
    if (!result)
        shaping.m_logs.clear();

    return result;
}

void UniscribeHelper::fillShapes()
{
    m_shapes.resize(m_runs.size());
    for (size_t i = 0; i < m_runs.size(); i++) {
        int startItem = m_runs[i].iCharPos;
        int itemLength = m_inputLength - startItem;
        if (i < m_runs.size() - 1)
            itemLength = m_runs[i + 1].iCharPos - startItem;

        int numGlyphs;
        if (itemLength < UNISCRIBE_HELPER_STACK_CHARS) {
            // We'll start our buffer sizes with the current stack space
            // available in our buffers if the current input fits. As long as
            // it doesn't expand past that we'll save a lot of time mallocing.
            numGlyphs = UNISCRIBE_HELPER_STACK_CHARS;
        } else {
            // When the input doesn't fit, give up with the stack since it will
            // almost surely not be enough room (unless the input actually
            // shrinks, which is unlikely) and just start with the length
            // recommended by the Uniscribe documentation as a "usually fits"
            // size.
            numGlyphs = itemLength * 3 / 2 + 16;
        }

        // Convert a string to a glyph string trying the primary font, fonts in
        // the fallback list and then script-specific last resort font.
        Shaping& shaping = m_shapes[i];
        if (!shape(&m_input[startItem], itemLength, numGlyphs, m_runs[i], shaping))
            continue;

        // At the moment, the only time m_disableFontFallback is set is
        // when we look up glyph indices for non-BMP code ranges. So,
        // we can skip the glyph placement. When that becomes not the case
        // any more, we have to add a new flag to control glyph placement.
        if (m_disableFontFallback)
          continue;

        // Compute placements. Note that offsets is documented incorrectly
        // and is actually an array.

        // DC that we lazily create if Uniscribe commands us to.
        // (this does not happen often because scriptCache is already
        //  updated when calling ScriptShape).
        HDC tempDC = 0;
        HGDIOBJ oldFont = 0;
        HRESULT hr;
        while (true) {
            shaping.m_prePadding = 0;
            hr = ScriptPlace(tempDC, shaping.m_scriptCache,
                             &shaping.m_glyphs[0],
                             static_cast<int>(shaping.m_glyphs.size()),
                             &shaping.m_visualAttributes[0], &m_runs[i].a,
                             &shaping.m_advance[0], &shaping.m_offsets[0],
                             &shaping.m_abc);
            if (hr != E_PENDING)
                break;

            // Allocate the DC and run the loop again.
            tempDC = GetDC(0);
            oldFont = SelectObject(tempDC, shaping.m_hfont);
        }

        if (FAILED(hr)) {
            // Some error we don't know how to handle. Nuke all of our data
            // since we can't deal with partially valid data later.
            m_runs.clear();
            m_shapes.clear();
            m_screenOrder.clear();
        }

        if (tempDC) {
            SelectObject(tempDC, oldFont);
            ReleaseDC(0, tempDC);
        }
    }

    adjustSpaceAdvances();

    if (m_letterSpacing != 0 || m_wordSpacing != 0)
        applySpacing();
}

void UniscribeHelper::fillScreenOrder()
{
    m_screenOrder.resize(m_runs.size());

    // We assume that the input has only one text direction in it.
    // TODO(brettw) are we sure we want to keep this restriction?
    if (m_isRtl) {
        for (int i = 0; i < static_cast<int>(m_screenOrder.size()); i++)
            m_screenOrder[static_cast<int>(m_screenOrder.size()) - i - 1] = i;
    } else {
        for (int i = 0; i < static_cast<int>(m_screenOrder.size()); i++)
            m_screenOrder[i] = i;
    }
}

void UniscribeHelper::adjustSpaceAdvances()
{
    if (m_spaceWidth == 0)
        return;

    int spaceWidthWithoutLetterSpacing = m_spaceWidth - m_letterSpacing;

    // This mostly matches what WebKit's UniscribeController::shapeAndPlaceItem.
    for (size_t run = 0; run < m_runs.size(); run++) {
        Shaping& shaping = m_shapes[run];

        for (int i = 0; i < shaping.charLength(); i++) {
            if (!treatAsSpace(m_input[m_runs[run].iCharPos + i]))
                continue;

            int glyphIndex = shaping.m_logs[i];
            int currentAdvance = shaping.m_advance[glyphIndex];

            // currentAdvance does not include additional letter-spacing, but
            // space_width does. Here we find out how off we are from the
            // correct width for the space not including letter-spacing, then
            // just subtract that diff.
            int diff = currentAdvance - spaceWidthWithoutLetterSpacing;
            // The shaping can consist of a run of text, so only subtract the
            // difference in the width of the glyph.
            shaping.m_advance[glyphIndex] -= diff;
            shaping.m_abc.abcB -= diff;
        }
    }
}

void UniscribeHelper::applySpacing()
{
    for (size_t run = 0; run < m_runs.size(); run++) {
        Shaping& shaping = m_shapes[run];
        bool isRtl = m_runs[run].a.fRTL;

        if (m_letterSpacing != 0) {
            // RTL text gets padded to the left of each character. We increment
            // the run's advance to make this happen. This will be balanced out
            // by NOT adding additional advance to the last glyph in the run.
            if (isRtl)
                shaping.m_prePadding += m_letterSpacing;

            // Go through all the glyphs in this run and increase the "advance"
            // to account for letter spacing. We adjust letter spacing only on
            // cluster boundaries.
            //
            // This works for most scripts, but may have problems with some
            // indic scripts. This behavior is better than Firefox or IE for
            // Hebrew.
            for (int i = 0; i < shaping.glyphLength(); i++) {
                if (shaping.m_visualAttributes[i].fClusterStart) {
                    // Ick, we need to assign the extra space so that the glyph
                    // comes first, then is followed by the space. This is
                    // opposite for RTL.
                    if (isRtl) {
                        if (i != shaping.glyphLength() - 1) {
                            // All but the last character just get the spacing
                            // applied to their advance. The last character
                            // doesn't get anything,
                            shaping.m_advance[i] += m_letterSpacing;
                            shaping.m_abc.abcB += m_letterSpacing;
                        }
                    } else {
                        // LTR case is easier, we just add to the advance.
                        shaping.m_advance[i] += m_letterSpacing;
                        shaping.m_abc.abcB += m_letterSpacing;
                    }
                }
            }
        }

        // Go through all the characters to find whitespace and insert the
        // extra wordspacing amount for the glyphs they correspond to.
        if (m_wordSpacing != 0) {
            for (int i = 0; i < shaping.charLength(); i++) {
                if (!treatAsSpace(m_input[m_runs[run].iCharPos + i]))
                    continue;

                // The char in question is a word separator...
                int glyphIndex = shaping.m_logs[i];

                // Spaces will not have a glyph in Uniscribe, it will just add
                // additional advance to the character to the left of the
                // space. The space's corresponding glyph will be the character
                // following it in reading order.
                if (isRtl) {
                    // In RTL, the glyph to the left of the space is the same
                    // as the first glyph of the following character, so we can
                    // just increment it.
                    shaping.m_advance[glyphIndex] += m_wordSpacing;
                    shaping.m_abc.abcB += m_wordSpacing;
                } else {
                    // LTR is actually more complex here, we apply it to the
                    // previous character if there is one, otherwise we have to
                    // apply it to the leading space of the run.
                    if (glyphIndex == 0)
                        shaping.m_prePadding += m_wordSpacing;
                    else {
                        shaping.m_advance[glyphIndex - 1] += m_wordSpacing;
                        shaping.m_abc.abcB += m_wordSpacing;
                    }
                }
            }
        }  // m_wordSpacing != 0

        // Loop for next run...
    }
}

// The advance is the ABC width of the run
int UniscribeHelper::advanceForItem(int itemIndex) const
{
    int accum = 0;
    const Shaping& shaping = m_shapes[itemIndex];

    if (shaping.m_justify.size() == 0) {
        // Easy case with no justification, the width is just the ABC width of
        // the run. (The ABC width is the sum of the advances).
        return shaping.m_abc.abcA + shaping.m_abc.abcB +
               shaping.m_abc.abcC + shaping.m_prePadding;
    }

    // With justification, we use the justified amounts instead. The
    // justification array contains both the advance and the extra space
    // added for justification, so is the width we want.
    int justification = 0;
    for (size_t i = 0; i < shaping.m_justify.size(); i++)
        justification += shaping.m_justify[i];

    return shaping.m_prePadding + justification;
}

}  // namespace WebCore
