/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextCodec.h"
#include <unicode/uchar.h>
#include <wtf/unicode/CharacterNames.h>

#include <array>
#include <cstdio>

namespace PAL {

int TextCodec::getUnencodableReplacement(char32_t codePoint, UnencodableHandling handling, UnencodableReplacementArray& replacement)
{
    ASSERT(!(codePoint > UCHAR_MAX_VALUE));

    // The Encoding Standard doesn't have surrogate code points in the input, but that would require
    // scanning and potentially manipulating inputs ahead of time. Instead handle them at the last
    // possible point.
    if (U_IS_SURROGATE(codePoint))
        codePoint = replacementCharacter;

    switch (handling) {
    case UnencodableHandling::Entities:
        return snprintf(replacement.data(), sizeof(UnencodableReplacementArray), "&#%u;", static_cast<unsigned>(codePoint));
    case UnencodableHandling::URLEncodedEntities:
        return snprintf(replacement.data(), sizeof(UnencodableReplacementArray), "%%26%%23%u%%3B", static_cast<unsigned>(codePoint));
    }
    ASSERT_NOT_REACHED();
    replacement.data()[0] = 0;
    return 0;
}

} // namespace PAL
