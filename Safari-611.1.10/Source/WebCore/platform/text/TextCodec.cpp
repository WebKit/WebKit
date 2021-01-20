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

#include <array>
#include <cstdio>

namespace WebCore {

int TextCodec::getUnencodableReplacement(UChar32 codePoint, UnencodableHandling handling, UnencodableReplacementArray& replacement)
{
    switch (handling) {
    case UnencodableHandling::QuestionMarks:
        replacement.data()[0] = '?';
        replacement.data()[1] = 0;
        return 1;
    case UnencodableHandling::Entities:
        return snprintf(replacement.data(), sizeof(UnencodableReplacementArray), "&#%u;", codePoint);
    case UnencodableHandling::URLEncodedEntities:
        return snprintf(replacement.data(), sizeof(UnencodableReplacementArray), "%%26%%23%u%%3B", codePoint);
    }
    ASSERT_NOT_REACHED();
    replacement.data()[0] = 0;
    return 0;
}

} // namespace WebCore
