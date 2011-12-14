/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#ifndef SkiaFontWin_h
#define SkiaFontWin_h

#include <windows.h>
#include <usp10.h>

class SkPath;
class SkPoint;

namespace WebCore {

class GraphicsContext;
class PlatformContextSkia;

// FIXME: Rename file to SkiaWinOutlineCache
class SkiaWinOutlineCache {
public:
    static const SkPath* lookupOrCreatePathForGlyph(HDC, HFONT, WORD);
    // Removes any cached glyphs from the outline cache corresponding to the
    // given font handle.
    static void removePathsForFont(HFONT);

private:
    SkiaWinOutlineCache();
};

// The functions below are used for more complex font drawing (effects such as
// stroking and more complex transforms) than Windows supports directly.  Since 
// Windows drawing is faster you should use windowsCanHandleTextDrawing first to 
// check if using Skia is required at all.
// Note that the text will look different (no ClearType) so this should only be
// used when necessary.
//
// When you call a Skia* text drawing function, various glyph outlines will be
// cached. As a result, you should call SkiaWinOutlineCache::removePathsForFont
// when the font is destroyed so that the cache does not outlive the font (since
// the HFONTs are recycled).
//
// Remember that Skia's text drawing origin is the baseline, like WebKit, not
// the top, like Windows.

// Returns true if the fillColor and shadowColor are opaque and the text-shadow
// is not blurred.
bool windowsCanHandleDrawTextShadow(GraphicsContext*);

// Returns true if advanced font rendering is recommended.
bool windowsCanHandleTextDrawing(GraphicsContext*);

// Returns true if advanced font rendering is recommended if shadows are
// disregarded.
bool windowsCanHandleTextDrawingWithoutShadow(GraphicsContext*);

// Note that the offsets parameter is optional.  If not NULL it represents a
// per glyph offset (such as returned by ScriptPlace Windows API function).
//
// Returns true of the text was drawn successfully. False indicates an error
// from Windows.
bool paintSkiaText(GraphicsContext* graphicsContext,
                   HFONT hfont,
                   int numGlyphs,
                   const WORD* glyphs,
                   const int* advances,
                   const GOFFSET* offsets,
                   const SkPoint* origin);

}  // namespace WebCore

#endif  // SkiaFontWin_h
