// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2022 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSAtRuleID.h"

#include <wtf/SortedArrayMap.h>

namespace WebCore {

CSSAtRuleID cssAtRuleID(StringView name)
{
    static constexpr std::pair<ComparableLettersLiteral, CSSAtRuleID> mappings[] = {
        { "-webkit-keyframes", CSSAtRuleWebkitKeyframes },
        { "annotation", CSSAtRuleAnnotation },
        { "character-variant", CSSAtRuleCharacterVariant },
        { "charset", CSSAtRuleCharset },
        { "container", CSSAtRuleContainer },
        { "counter-style", CSSAtRuleCounterStyle },
        { "font-face", CSSAtRuleFontFace },
        { "font-feature-values", CSSAtRuleFontFeatureValues },
        { "font-palette-values", CSSAtRuleFontPaletteValues },
        { "import", CSSAtRuleImport },
        { "keyframes", CSSAtRuleKeyframes },
        { "layer", CSSAtRuleLayer },
        { "media", CSSAtRuleMedia },
        { "namespace", CSSAtRuleNamespace },
        { "ornaments", CSSAtRuleOrnaments },
        { "page", CSSAtRulePage },
        { "styleset", CSSAtRuleStyleset },
        { "stylistic", CSSAtRuleStylistic },
        { "supports", CSSAtRuleSupports },
        { "swash", CSSAtRuleSwash },
        { "viewport", CSSAtRuleViewport },
    };
    static constexpr SortedArrayMap cssAtRules { mappings };
    return cssAtRules.get(name, CSSAtRuleInvalid);
}

} // namespace WebCore

