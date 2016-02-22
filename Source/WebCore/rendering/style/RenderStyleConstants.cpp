/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
#include "RenderStyleConstants.h"

#include "TextStream.h"

namespace WebCore {

TextStream& operator<<(TextStream& ts, EFillSizeType sizeType)
{
    switch (sizeType) {
    case Contain: ts << "contain"; break;
    case Cover: ts << "cover"; break;
    case SizeLength: ts << "size-length"; break;
    case SizeNone: ts << "size-none"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, EFillAttachment attachment)
{
    switch (attachment) {
    case ScrollBackgroundAttachment: ts << "scroll"; break;
    case LocalBackgroundAttachment: ts << "local"; break;
    case FixedBackgroundAttachment: ts << "fixed"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EFillBox fill)
{
    switch (fill) {
    case BorderFillBox: ts << "border"; break;
    case PaddingFillBox: ts << "padding"; break;
    case ContentFillBox: ts << "content"; break;
    case TextFillBox: ts << "text"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, EFillRepeat repeat)
{
    switch (repeat) {
    case RepeatFill: ts << "repeat"; break;
    case NoRepeatFill: ts << "no-repeat"; break;
    case RoundFill: ts << "round"; break;
    case SpaceFill: ts << "space"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, EMaskSourceType maskSource)
{
    switch (maskSource) {
    case MaskAlpha: ts << "alpha"; break;
    case MaskLuminance: ts << "luminance"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, Edge edge)
{
    switch (edge) {
    case Edge::Top: ts << "top"; break;
    case Edge::Right: ts << "right"; break;
    case Edge::Bottom: ts << "bottom"; break;
    case Edge::Left: ts << "left"; break;
    }
    return ts;
}

bool alwaysPageBreak(BreakBetween between)
{
    return between >= PageBreakBetween;
}

} // namespace WebCore
