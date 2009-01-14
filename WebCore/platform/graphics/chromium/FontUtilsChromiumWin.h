/*
 * Copyright (c) 2006, 2007, 2008, Google Inc. All rights reserved.
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

// A collection of utilities for font handling.

// FIXME: Move all methods to the files that have their callsites and remove this file.
// *Utils files are not very WebKit-ty.

#ifndef FontUtilsWin_h
#define FontUtilsWin_h

#include <usp10.h>
#include <wchar.h>
#include <windows.h>

#include "FontDescription.h"
#include <unicode/uscript.h>

namespace WebCore {

// Return a font family that supports a script and belongs to |generic| font
// family.  It can return NULL and a caller has to implement its own fallback.
const UChar* getFontFamilyForScript(UScriptCode, FontDescription::GenericFamilyType);

// Return a font family that can render |characters| based on
// what script characters belong to. When char_checked is non-NULL,
// it's filled with the character used to determine the script.
// When script_checked is non-NULL, the script used to determine
// the family is returned.
// FIXME: This function needs a total overhaul.
const UChar* getFallbackFamily(const UChar* characters, int length,
                               FontDescription::GenericFamilyType,
                               UChar32* charChecked,
                               UScriptCode* scriptChecked);

// Derive a new HFONT by replacing lfFaceName of LOGFONT with |family|,
// calculate the ascent for the derived HFONT, and initialize SCRIPT_CACHE
// in FontData.
// |style| is only used for cache key generation. |style| is
// bit-wise OR of BOLD(1), UNDERLINED(2) and ITALIC(4) and
// should match what's contained in LOGFONT. It should be calculated
// by calling GetStyleFromLogFont.
// Returns false if the font is not accessible, in which case |ascent| field
// of |fontdata| is set to kUndefinedAscent.
// Be aware that this is not thread-safe.
// FIXME: Instead of having three out params, we'd better have one
// (|*FontData|), but somehow it mysteriously messes up the layout for
// certain complex script pages (e.g. hi.wikipedia.org) and also crashes
// at the start-up if recently visited page list includes pages with complex
// scripts in their title. Moreover, somehow the very first-pass of
// intl2 page-cycler test is noticeably slower with one out param than
// the current version although the subsequent 9 passes take about the
// same time.
bool getDerivedFontData(const UChar* family, int style, LOGFONT*, int* ascent, HFONT*, SCRIPT_CACHE**);

enum {
    FontStyleNormal = 0,
    FontStyleBold = 1,
    FontStyleItalic = 2,
    FontStyleUnderlined = 4
};

// Derive style (bit-wise OR of FONT_STYLE_BOLD, FONT_STYLE_UNDERLINED, and
// FONT_STYLE_ITALIC) from LOGFONT. Returns 0 if |*logfont| is NULL.
int getStyleFromLogfont(const LOGFONT*);

}  // namespace WebCore

#endif  // FontUtilsWin_h
