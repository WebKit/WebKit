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

#ifndef WebSandboxSupport_h
#define WebSandboxSupport_h

#include "../WebCommon.h"
#include "../WebString.h"

namespace WebKit {

struct WebFontRenderStyle;

// Put methods here that are required due to sandbox restrictions.
class WebSandboxSupport {
public:
    // Fonts ---------------------------------------------------------------

    // Get a font family which contains glyphs for the given Unicode
    // code-points.
    //   characters: a UTF-16 encoded string
    //   numCharacters: the number of 16-bit words in |characters|
    //   preferredLocale: preferred locale identifier for the |characters|
    //                    (e.g. "en", "ja", "zh-CN")
    //
    // Returns a string with the font family on an empty string if the
    // request cannot be satisfied.
    virtual WebString getFontFamilyForCharacters(const WebUChar* characters, size_t numCharacters, const char* preferredLocale)  = 0;
    virtual void getRenderStyleForStrike(const char* family, int sizeAndStyle, WebFontRenderStyle* style) = 0;
};

} // namespace WebKit

#endif
