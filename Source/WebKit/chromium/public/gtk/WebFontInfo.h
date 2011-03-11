/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebFontInfo_h
#define WebFontInfo_h

#include "../WebCString.h"
#include "../linux/WebFontRenderStyle.h"

#include <string.h>
#include <unistd.h>

namespace WebKit {

class WebFontInfo {
public:
    // Return a font family which provides glyphs for the Unicode code points
    // specified by |utf16|
    //   characters: a native-endian UTF16 string
    //   numCharacters: the number of 16-bit words in |utf16|
    //   preferredLocale: preferred locale identifier for the |characters|
    //                    (e.g. "en", "ja", "zh-CN")
    //
    // Returns: the font family or an empty string if the request could not be
    // satisfied.
    WEBKIT_API static WebCString familyForChars(const WebUChar* characters, size_t numCharacters, const char* preferredLocale);

    // Fill out the given WebFontRenderStyle with the user's preferences for
    // rendering the given font at the given size.
    //   family: i.e. "Times New Roman"
    //   sizeAndStyle:
    //      3322222222221111111111
    //      10987654321098765432109876543210
    //     +--------------------------------+
    //     |..............Size............IB|
    //     +--------------------------------+
    //     I: italic flag
    //     B: bold flag
    WEBKIT_API static void renderStyleForStrike(const char* family, int sizeAndStyle, WebFontRenderStyle* result);
};

} // namespace WebKit

#endif
