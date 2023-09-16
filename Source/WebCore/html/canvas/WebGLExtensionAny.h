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

#pragma once

#if ENABLE(WEBGL)

#include <variant>
#include <wtf/Ref.h>

namespace WebCore {

class ANGLEInstancedArrays;
class EXTBlendFuncExtended;
class EXTBlendMinMax;
class EXTClipControl;
class EXTColorBufferFloat;
class EXTColorBufferHalfFloat;
class EXTConservativeDepth;
class EXTDepthClamp;
class EXTDisjointTimerQuery;
class EXTDisjointTimerQueryWebGL2;
class EXTFloatBlend;
class EXTFragDepth;
class EXTPolygonOffsetClamp;
class EXTRenderSnorm;
class EXTShaderTextureLOD;
class EXTTextureCompressionBPTC;
class EXTTextureCompressionRGTC;
class EXTTextureFilterAnisotropic;
class EXTTextureMirrorClampToEdge;
class EXTTextureNorm16;
class EXTsRGB;
class KHRParallelShaderCompile;
class NVShaderNoperspectiveInterpolation;
class OESDrawBuffersIndexed;
class OESElementIndexUint;
class OESFBORenderMipmap;
class OESSampleVariables;
class OESShaderMultisampleInterpolation;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class OESVertexArrayObject;
class WebGLClipCullDistance;
class WebGLColorBufferFloat;
class WebGLCompressedTextureASTC;
class WebGLCompressedTextureETC;
class WebGLCompressedTextureETC1;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
class WebGLCompressedTextureS3TCsRGB;
class WebGLDebugRendererInfo;
class WebGLDebugShaders;
class WebGLDepthTexture;
class WebGLDrawBuffers;
class WebGLDrawInstancedBaseVertexBaseInstance;
class WebGLLoseContext;
class WebGLMultiDraw;
class WebGLMultiDrawInstancedBaseVertexBaseInstance;
class WebGLPolygonMode;
class WebGLProvokingVertex;
class WebGLRenderSharedExponent;
class WebGLStencilTexturing;

// Variant reprenting any WebGL extension.
// Remember to #include "WebGLExtensionAnyInlines.h" at any instantiation compilation unit.
using WebGLExtensionAny = std::variant<
    Ref<ANGLEInstancedArrays>,
    Ref<EXTBlendFuncExtended>,
    Ref<EXTBlendMinMax>,
    Ref<EXTClipControl>,
    Ref<EXTColorBufferFloat>,
    Ref<EXTColorBufferHalfFloat>,
    Ref<EXTConservativeDepth>,
    Ref<EXTDepthClamp>,
    Ref<EXTDisjointTimerQuery>,
    Ref<EXTDisjointTimerQueryWebGL2>,
    Ref<EXTFloatBlend>,
    Ref<EXTFragDepth>,
    Ref<EXTPolygonOffsetClamp>,
    Ref<EXTRenderSnorm>,
    Ref<EXTShaderTextureLOD>,
    Ref<EXTTextureCompressionBPTC>,
    Ref<EXTTextureCompressionRGTC>,
    Ref<EXTTextureFilterAnisotropic>,
    Ref<EXTTextureMirrorClampToEdge>,
    Ref<EXTTextureNorm16>,
    Ref<EXTsRGB>,
    Ref<KHRParallelShaderCompile>,
    Ref<NVShaderNoperspectiveInterpolation>,
    Ref<OESDrawBuffersIndexed>,
    Ref<OESElementIndexUint>,
    Ref<OESFBORenderMipmap>,
    Ref<OESSampleVariables>,
    Ref<OESShaderMultisampleInterpolation>,
    Ref<OESStandardDerivatives>,
    Ref<OESTextureFloat>,
    Ref<OESTextureFloatLinear>,
    Ref<OESTextureHalfFloat>,
    Ref<OESTextureHalfFloatLinear>,
    Ref<OESVertexArrayObject>,
    Ref<WebGLClipCullDistance>,
    Ref<WebGLColorBufferFloat>,
    Ref<WebGLCompressedTextureASTC>,
    Ref<WebGLCompressedTextureETC>,
    Ref<WebGLCompressedTextureETC1>,
    Ref<WebGLCompressedTexturePVRTC>,
    Ref<WebGLCompressedTextureS3TC>,
    Ref<WebGLCompressedTextureS3TCsRGB>,
    Ref<WebGLDebugRendererInfo>,
    Ref<WebGLDebugShaders>,
    Ref<WebGLDepthTexture>,
    Ref<WebGLDrawBuffers>,
    Ref<WebGLDrawInstancedBaseVertexBaseInstance>,
    Ref<WebGLLoseContext>,
    Ref<WebGLMultiDraw>,
    Ref<WebGLMultiDrawInstancedBaseVertexBaseInstance>,
    Ref<WebGLPolygonMode>,
    Ref<WebGLProvokingVertex>,
    Ref<WebGLRenderSharedExponent>,
    Ref<WebGLStencilTexturing>
    >;
}

#endif
