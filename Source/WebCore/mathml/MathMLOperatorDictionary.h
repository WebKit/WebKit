/*
 * Copyright (C) 2015 Frederic Wang (fred.wang@free.fr). All rights reserved.
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

#pragma once

#if ENABLE(MATHML)

#include <unicode/utypes.h>
#include <wtf/Forward.h>

namespace WebCore {

namespace MathMLOperatorDictionary {
enum Form { Infix, Prefix, Postfix };
enum Flag {
    Accent = 0x1,
    Fence = 0x2, // This has no visual effect but allows to expose semantic information via the accessibility tree.
    LargeOp = 0x4,
    MovableLimits = 0x8,
    Separator = 0x10, // This has no visual effect but allows to expose semantic information via the accessibility tree.
    Stretchy = 0x20,
    Symmetric = 0x40
};
const unsigned allFlags = Accent | Fence | LargeOp | MovableLimits | Separator | Stretchy | Symmetric;
struct Property {
    MathMLOperatorDictionary::Form form;
    // Default leading and trailing spaces are "thickmathspace".
    unsigned short leadingSpaceInMathUnit { 5 };
    unsigned short trailingSpaceInMathUnit { 5 };
    // Default operator properties are all set to "false".
    unsigned short flags { 0 };
};
Optional<Property> search(UChar32, Form, bool explicitForm);
bool isVertical(UChar32);
}

}
#endif // ENABLE(MATHML)
