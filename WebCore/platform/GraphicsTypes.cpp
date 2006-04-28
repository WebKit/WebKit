/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsTypes.h"

#include "PlatformString.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

static const char* const compositeOperatorNames[] = {
    "clear",
    "copy",
    "source-over",
    "source-in",
    "source-out",
    "source-atop",
    "destination-over",
    "destination-in",
    "destination-out",
    "destination-atop",
    "xor",
    "darker",
    "highlight",
    "lighter"
};
const int numCompositeOperatorNames = sizeof(compositeOperatorNames) / sizeof(compositeOperatorNames[0]);

bool parseCompositeOperator(const String& s, CompositeOperator& op)
{
    for (int i = 0; i < numCompositeOperatorNames; i++)
        if (s == compositeOperatorNames[i]) {
            op = static_cast<CompositeOperator>(i);
            return true;
        }
    return false;
}

String compositeOperatorName(CompositeOperator op)
{
    ASSERT(op >= 0);
    ASSERT(op < numCompositeOperatorNames);
    return compositeOperatorNames[op];
}

bool parseLineCap(const String& s, LineCap& cap)
{
    if (s == "butt") {
        cap = ButtCap;
        return true;
    }
    if (s == "round") {
        cap = RoundCap;
        return true;
    }
    if (s == "square") {
        cap = SquareCap;
        return true;
    }
    return false;
}

String lineCapName(LineCap cap)
{
    ASSERT(cap >= 0);
    ASSERT(cap < 3);
    const char* const names[3] = { "butt", "round", "square" };
    return names[cap];
}

bool parseLineJoin(const String& s, LineJoin& join)
{
    if (s == "miter") {
        join = MiterJoin;
        return true;
    }
    if (s == "round") {
        join = RoundJoin;
        return true;
    }
    if (s == "bevel") {
        join = BevelJoin;
        return true;
    }
    return false;
}

String lineJoinName(LineJoin join)
{
    ASSERT(join >= 0);
    ASSERT(join < 3);
    const char* const names[3] = { "miter", "round", "bevel" };
    return names[join];
}

}
