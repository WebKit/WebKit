/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "GraphicsStyle.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, const GraphicsDropShadow& dropShadow)
{
    ts.dumpProperty("offset", dropShadow.offset);
    ts.dumpProperty("radius", dropShadow.radius);
    ts.dumpProperty("color", dropShadow.color);
    return ts;
}

TextStream& operator<<(TextStream& ts, const GraphicsGaussianBlur& gaussianBlur)
{
    ts.dumpProperty("radius", gaussianBlur.radius);
    return ts;
}

TextStream& operator<<(TextStream& ts, const GraphicsColorMatrix& colorMatrix)
{
    ts.dumpProperty("values", colorMatrix.values);
    return ts;
}

TextStream& operator<<(TextStream& ts, const GraphicsStyle& style)
{
    WTF::switchOn(style,
        [&] (const GraphicsDropShadow& dropShadow) {
            ts << dropShadow;
        },
        [&] (const GraphicsGaussianBlur& gaussianBlur) {
            ts << gaussianBlur;
        },
        [&] (const GraphicsColorMatrix& colorMatrix) {
            ts << colorMatrix;
        }
    );
    return ts;
}

} // namespace WebCore
