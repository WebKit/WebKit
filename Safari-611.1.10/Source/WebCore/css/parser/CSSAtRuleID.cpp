// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
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

namespace WebCore {

CSSAtRuleID cssAtRuleID(StringView name)
{
    if (equalIgnoringASCIICase(name, "charset"))
        return CSSAtRuleCharset;
    if (equalIgnoringASCIICase(name, "font-face"))
        return CSSAtRuleFontFace;
    if (equalIgnoringASCIICase(name, "import"))
        return CSSAtRuleImport;
    if (equalIgnoringASCIICase(name, "keyframes"))
        return CSSAtRuleKeyframes;
    if (equalIgnoringASCIICase(name, "media"))
        return CSSAtRuleMedia;
    if (equalIgnoringASCIICase(name, "namespace"))
        return CSSAtRuleNamespace;
    if (equalIgnoringASCIICase(name, "page"))
        return CSSAtRulePage;
    if (equalIgnoringASCIICase(name, "supports"))
        return CSSAtRuleSupports;
    if (equalIgnoringASCIICase(name, "viewport"))
        return CSSAtRuleViewport;
    if (equalIgnoringASCIICase(name, "-webkit-keyframes"))
        return CSSAtRuleWebkitKeyframes;
    if (equalIgnoringASCIICase(name, "apply"))
        return CSSAtRuleApply;
    return CSSAtRuleInvalid;
}

} // namespace WebCore

