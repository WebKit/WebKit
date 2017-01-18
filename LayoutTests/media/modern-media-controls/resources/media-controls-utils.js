/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

function rgba(colorString)
{
    let [r, g, b, a] = [0, 0, 0, 0];
    const match = colorString.match(/^(?:rgba\(([^)]+)\))$/i);
    if (match && match[1]) {
        const rgba = match[1].split(/\s*,\s*/).map(parseFloat);
        if (rgba.length == 4) {
            [r, g, b] = rgba;
            a = Math.max(0, Math.min(rgba[3], 1));
        }
    }
    return { r, g, b, a };
}

function shouldBeEqualToRGBAColor(expr, expectedColor)
{
    const expectedRGBA = rgba(expectedColor);
    shouldBe(`rgba(${expr}).r`, `${expectedRGBA.r}`);
    shouldBe(`rgba(${expr}).g`, `${expectedRGBA.g}`);
    shouldBe(`rgba(${expr}).b`, `${expectedRGBA.b}`);
    shouldBeCloseTo(`rgba(${expr}).a`, expectedRGBA.a, 0.001);
}


