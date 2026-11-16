/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

class ShaderModule;
struct ConstantValue;
struct DeviceState;
struct PrepareResult;

namespace Metal {

void emitMetalFunctions(StringBuilder&, ShaderModule&, PrepareResult&, const HashMap<String, ConstantValue>&, DeviceState&&);

// The HDR to SDR mapping which importExternalTexture() applies to a PQ or HLG frame in the shader it
// is sampled by, and which copyExternalImageToTexture() applies in the shader it copies with. They
// are the same Metal Shading Language, so that a frame copied out of a video matches the same frame
// sampled through an external texture. PQ or HLG is decoded to linear light where 1.0 is 440 nits,
// what is brighter than half of that is rolled off towards 1.0 by luminance, and the result is
// encoded with the pure 2.4 gamma of ITU-R BT.1886 rather than the sRGB curve. Together these match
// how CoreGraphics draws the same frame to a 2D canvas.
//
// Self-contained and needing only <metal_stdlib> with `using namespace metal`. Where it is emitted
// ahead of use is up to the shader. Its identifiers begin with two underscores, which WGSL rejects,
// so that they cannot collide with the shader's own, and which is why it is spelled as quoted text in
// the implementation, where the style checker does not look.
ASCIILiteral hdrToSDRMetalSource();

} // namespace Metal
} // namespace WGSL
