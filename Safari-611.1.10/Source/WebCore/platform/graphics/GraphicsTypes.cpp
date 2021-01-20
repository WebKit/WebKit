/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2012 Rik Cabanier (cabanier@adobe.com)
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "GraphicsTypes.h"

#include "AlphaPremultiplication.h"
#include <wtf/Assertions.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

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
    "lighter",
    "difference"
};

static const char* const blendOperatorNames[] = {
    "normal",
    "multiply",
    "screen",
    "darken",
    "lighten",
    "overlay",
    "color-dodge",
    "color-burn",
    "hard-light",
    "soft-light",
    "difference",
    "exclusion",
    "hue",
    "saturation",
    "color",
    "luminosity",
    "plus-darker",
    "plus-lighter"
};
const uint8_t numCompositeOperatorNames = WTF_ARRAY_LENGTH(compositeOperatorNames);
const uint8_t numBlendOperatorNames = WTF_ARRAY_LENGTH(blendOperatorNames);

bool parseBlendMode(const String& s, BlendMode& blendMode)
{
    for (unsigned i = 0; i < numBlendOperatorNames; i++) {
        if (s == blendOperatorNames[i]) {
            blendMode = static_cast<BlendMode>(i + static_cast<unsigned>(BlendMode::Normal));
            return true;
        }
    }
    
    return false;
}

bool parseCompositeAndBlendOperator(const String& s, CompositeOperator& op, BlendMode& blendOp)
{
    for (int i = 0; i < numCompositeOperatorNames; i++) {
        if (s == compositeOperatorNames[i]) {
            op = static_cast<CompositeOperator>(i);
            blendOp = BlendMode::Normal;
            return true;
        }
    }
    
    if (parseBlendMode(s, blendOp)) {
        // For now, blending will always assume source-over. This will be fixed in the future
        op = CompositeOperator::SourceOver;
        return true;
    }
    
    return false;
}

// FIXME: when we support blend modes in combination with compositing other than source-over
// this routine needs to be updated.
String compositeOperatorName(CompositeOperator op, BlendMode blendOp)
{
    ASSERT(op >= CompositeOperator::Clear);
    ASSERT(static_cast<uint8_t>(op) < numCompositeOperatorNames);
    ASSERT(blendOp >= BlendMode::Normal);
    ASSERT(static_cast<uint8_t>(blendOp) <= numBlendOperatorNames);
    if (blendOp > BlendMode::Normal)
        return blendOperatorNames[static_cast<unsigned>(blendOp) - static_cast<unsigned>(BlendMode::Normal)];
    return compositeOperatorNames[static_cast<unsigned>(op)];
}

String blendModeName(BlendMode blendOp)
{
    ASSERT(blendOp >= BlendMode::Normal);
    ASSERT(blendOp <= BlendMode::PlusLighter);
    return blendOperatorNames[static_cast<unsigned>(blendOp) - static_cast<unsigned>(BlendMode::Normal)];
}

TextStream& operator<<(TextStream& ts, CompositeOperator op)
{
    return ts << compositeOperatorName(op, BlendMode::Normal);
}

TextStream& operator<<(TextStream& ts, BlendMode blendMode)
{
    return ts << blendModeName(blendMode);
}

TextStream& operator<<(TextStream& ts, WindRule rule)
{
    switch (rule) {
    case WindRule::NonZero:
        ts << "NON-ZERO";
        break;
    case WindRule::EvenOdd:
        ts << "EVEN-ODD";
        break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, LineCap capStyle)
{
    switch (capStyle) {
    case ButtCap:
        ts << "BUTT";
        break;
    case RoundCap:
        ts << "ROUND";
        break;
    case SquareCap:
        ts << "SQUARE";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineJoin joinStyle)
{
    switch (joinStyle) {
    case MiterJoin:
        ts << "MITER";
        break;
    case RoundJoin:
        ts << "ROUND";
        break;
    case BevelJoin:
        ts << "BEVEL";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, AlphaPremultiplication premultiplication)
{
    switch (premultiplication) {
    case AlphaPremultiplication::Premultiplied:
        ts << "premultiplied";
        break;
    case AlphaPremultiplication::Unpremultiplied:
        ts << "unpremultiplied";
        break;
    }
    return ts;
}

} // namespace WebCore
