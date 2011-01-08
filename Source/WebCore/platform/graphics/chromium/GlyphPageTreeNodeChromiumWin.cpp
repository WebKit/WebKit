/*
 * Copyright (c) 2008, 2009 Google Inc. All rights reserved.
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
#include <windows.h>
#include <vector>

#include "ChromiumBridge.h"
#include "Font.h"
#include "GlyphPageTreeNode.h"
#include "SimpleFontData.h"
#include "UniscribeHelperTextRun.h"
#include "WindowsVersion.h"

namespace WebCore {

// Fills one page of font data pointers with 0 to indicate that there
// are no glyphs for the characters.
static void fillEmptyGlyphs(GlyphPage* page)
{
    for (int i = 0; i < GlyphPage::size; ++i)
        page->setGlyphDataForIndex(i, 0, 0);
}

// Lazily initializes space glyph
static Glyph initSpaceGlyph(HDC dc, Glyph* spaceGlyph)
{
    if (*spaceGlyph)
        return *spaceGlyph;

    static wchar_t space = ' ';
    GetGlyphIndices(dc, &space, 1, spaceGlyph, 0);
    return *spaceGlyph;
}

// Fills |length| glyphs starting at |offset| in a |page| in the Basic 
// Multilingual Plane (<= U+FFFF). The input buffer size should be the
// same as |length|. We can use the standard Windows GDI functions here. 
// Returns true if any glyphs were found.
static bool fillBMPGlyphs(unsigned offset,
                          unsigned length,
                          UChar* buffer,
                          GlyphPage* page,
                          const SimpleFontData* fontData,
                          bool recurse)
{
    HDC dc = GetDC((HWND)0);
    HGDIOBJ oldFont = SelectObject(dc, fontData->platformData().hfont());

    TEXTMETRIC tm = {0};
    if (!GetTextMetrics(dc, &tm)) {
        SelectObject(dc, oldFont);
        ReleaseDC(0, dc);

        if (recurse) {
            if (ChromiumBridge::ensureFontLoaded(fontData->platformData().hfont()))
                return fillBMPGlyphs(offset, length, buffer, page, fontData, false);
            else {
                fillEmptyGlyphs(page);
                return false;
            }
        } else {
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401
            LOG_ERROR("Unable to get the text metrics after second attempt");
            fillEmptyGlyphs(page);
            return false;
        }
    }

    // FIXME: GetGlyphIndices() sets each item of localGlyphBuffer[]
    // with the one of the values listed below.
    //  * With the GGI_MARK_NONEXISTING_GLYPHS flag
    //    + If the font has a glyph available for the character,
    //      localGlyphBuffer[i] > 0x0.
    //    + If the font does not have glyphs available for the character,
    //      localGlyphBuffer[i] = 0x1F (TrueType Collection?) or
    //                            0xFFFF (OpenType?).
    //  * Without the GGI_MARK_NONEXISTING_GLYPHS flag
    //    + If the font has a glyph available for the character,
    //      localGlyphBuffer[i] > 0x0.
    //    + If the font does not have glyphs available for the character,
    //      localGlyphBuffer[i] = 0x80.
    //      (Windows automatically assigns the glyph for a box character to
    //      prevent ExtTextOut() from returning errors.)
    // To avoid from hurting the rendering performance, this code just
    // tells WebKit whether or not the all glyph indices for the given
    // characters are 0x80 (i.e. a possibly-invalid glyph) and let it
    // use alternative fonts for the characters.
    // Although this may cause a problem, it seems to work fine as far as I
    // have tested. (Obviously, I need more tests.)
    WORD localGlyphBuffer[GlyphPage::size];

    // FIXME: I find some Chinese characters can not be correctly displayed
    // when call GetGlyphIndices without flag GGI_MARK_NONEXISTING_GLYPHS,
    // because the corresponding glyph index is set as 0x20 when current font
    // does not have glyphs available for the character. According a blog post
    // http://blogs.msdn.com/michkap/archive/2006/06/28/649791.aspx
    // I think we should switch to the way about calling GetGlyphIndices with
    // flag GGI_MARK_NONEXISTING_GLYPHS, it should be OK according the
    // description of MSDN.
    // Also according to Jungshik and Hironori's suggestion and modification
    // we treat turetype and raster Font as different way when windows version
    // is less than Vista.
    GetGlyphIndices(dc, buffer, length, localGlyphBuffer, GGI_MARK_NONEXISTING_GLYPHS);

    // Copy the output to the GlyphPage
    bool haveGlyphs = false;
    int invalidGlyph = 0xFFFF;
    const DWORD cffTableTag = 0x20464643; // 4-byte identifier for OpenType CFF table ('CFF ').
    if (!isVistaOrNewer() && !(tm.tmPitchAndFamily & TMPF_TRUETYPE) && (GetFontData(dc, cffTableTag, 0, 0, 0) == GDI_ERROR))
        invalidGlyph = 0x1F;

    Glyph spaceGlyph = 0;  // Glyph for a space. Lazily filled.

    for (unsigned i = 0; i < length; i++) {
        UChar c = buffer[i];
        Glyph glyph = localGlyphBuffer[i];
        const SimpleFontData* glyphFontData = fontData;
        // When this character should be a space, we ignore whatever the font
        // says and use a space. Otherwise, if fonts don't map one of these
        // space or zero width glyphs, we will get a box.
        if (Font::treatAsSpace(c)) {
            // Hard code the glyph indices for characters that should be
            // treated like spaces.
            glyph = initSpaceGlyph(dc, &spaceGlyph);
        } else if (glyph == invalidGlyph) {
            // WebKit expects both the glyph index and FontData
            // pointer to be 0 if the glyph is not present
            glyph = 0;
            glyphFontData = 0;
        } else
            haveGlyphs = true;
        page->setGlyphDataForCharacter(offset + i, glyph, glyphFontData);
    }

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);
    return haveGlyphs;
}

// For non-BMP characters, each is two words (UTF-16) and the input buffer
// size is 2 * |length|. Since GDI doesn't know how to handle non-BMP
// characters, we must use Uniscribe to tell us the glyph indices.
//
// We don't want to call this in the case of "regular" characters since some
// fonts may not have the correct combining rules for accents. See the notes
// at the bottom of ScriptGetCMap. We can't use ScriptGetCMap, though, since
// it doesn't seem to support UTF-16, despite what this blog post says:
//   http://blogs.msdn.com/michkap/archive/2006/06/29/650680.aspx
//
// So we fire up the full Uniscribe doohicky, give it our string, and it will
// correctly handle the UTF-16 for us. The hard part is taking this and getting
// the glyph indices back out that correspond to the correct input characters,
// since they may be missing.
//
// Returns true if any glyphs were found.
static bool fillNonBMPGlyphs(unsigned offset,
                             unsigned length,
                             UChar* buffer,
                             GlyphPage* page,
                             const SimpleFontData* fontData)
{
    bool haveGlyphs = false;

    UniscribeHelperTextRun state(buffer, length * 2, false,
                                 fontData->platformData().hfont(),
                                 fontData->platformData().scriptCache(),
                                 fontData->platformData().scriptFontProperties());
    state.setInhibitLigate(true);
    state.setDisableFontFallback(true);
    state.init();

    for (unsigned i = 0; i < length; i++) {
        // Each character in this input buffer is a surrogate pair, which
        // consists of two UChars. So, the offset for its i-th character is
        // (i * 2).
        WORD glyph = state.firstGlyphForCharacter(i * 2);
        if (glyph) {
            haveGlyphs = true;
            page->setGlyphDataForIndex(offset + i, glyph, fontData);
        } else
            // Clear both glyph and fontData fields.
            page->setGlyphDataForIndex(offset + i, 0, 0);
    }
    return haveGlyphs;
}

// We're supposed to return true if there are any glyphs in the range
// specified by |offset| and |length| in  our font,
// false if there are none.
bool GlyphPage::fill(unsigned offset, unsigned length, UChar* characterBuffer,
                     unsigned bufferLength, const SimpleFontData* fontData)
{
    // We have to handle BMP and non-BMP characters differently.
    // FIXME: Add assertions to make sure that buffer is entirely in BMP
    // or entirely in non-BMP. 
    if (bufferLength == length)
        return fillBMPGlyphs(offset, length, characterBuffer, this, fontData, true);

    if (bufferLength == 2 * length) {
        // A non-BMP input buffer will be twice as long as output glyph buffer
        // because each character in the non-BMP input buffer will be 
        // represented by a surrogate pair (two UChar's).
        return fillNonBMPGlyphs(offset, length, characterBuffer, this, fontData);
    }

    ASSERT_NOT_REACHED();
    return false;
}

}  // namespace WebCore
