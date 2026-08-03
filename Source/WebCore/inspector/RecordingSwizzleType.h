/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

namespace WebCore {

// Keep this in sync with WI.Recording.Swizzle.
enum class RecordingSwizzleType : int {
    None = 0,
    Number = 1,
    Boolean = 2,
    String = 3,
    Array = 4,
    TypedArray = 5,
    Image = 6,
    ImageData = 7,
    DOMMatrix = 8,
    Path2D = 9,
    CanvasGradient = 10,
    CanvasPattern = 11,
    WebGLBuffer = 12,
    WebGLFramebuffer = 13,
    WebGLRenderbuffer = 14,
    WebGLTexture = 15,
    WebGLShader = 16,
    WebGLProgram = 17,
    WebGLUniformLocation = 18,
    ImageBitmap = 19,
    WebGLQuery = 20,
    WebGLSampler = 21,
    WebGLSync = 22,
    WebGLTransformFeedback = 23,
    WebGLVertexArrayObject = 24,
    DOMPointInit = 25,
    Canvas = 26,
    GPUBindGroup = 27,
    GPUBindGroupLayout = 28,
    GPUBuffer = 29,
    GPUCommandBuffer = 30,
    GPUCommandEncoder = 31,
    GPUComputePassEncoder = 32,
    GPUComputePipeline = 33,
    GPUExternalTexture = 34,
    GPUPipelineLayout = 35,
    GPUQuerySet = 36,
    GPUQueue = 37,
    GPURenderBundle = 38,
    GPURenderBundleEncoder = 39,
    GPURenderPassEncoder = 40,
    GPURenderPipeline = 41,
    GPUSampler = 42,
    GPUShaderModule = 43,
    GPUTexture = 44,
    GPUTextureView = 45,
    WebGLBlendFuncExtended = 46,
    WebGLClipCullDistance = 47,
    WebGLColorBufferFloat = 48,
    WebGLCompressedTextureASTC = 49,
    WebGLCompressedTextureETC = 50,
    WebGLCompressedTextureETC1 = 51,
    WebGLCompressedTexturePVRTC = 52,
    WebGLCompressedTextureS3TC = 53,
    WebGLCompressedTextureS3TCsRGB = 54,
    WebGLDebugRendererInfo = 55,
    WebGLDebugShaders = 56,
    WebGLDepthTexture = 57,
    WebGLDrawBuffers = 58,
    WebGLDrawInstancedBaseVertexBaseInstance = 59,
    WebGLLoseContext = 60,
    WebGLMultiDraw = 61,
    WebGLMultiDrawInstancedBaseVertexBaseInstance = 62,
    WebGLPolygonMode = 63,
    WebGLProvokingVertex = 64,
    WebGLRenderSharedExponent = 65,
    WebGLStencilTexturing = 66,
    WebGLTimerQueryEXT = 67,
    WebGLVertexArrayObjectOES = 68,
    ANGLEInstancedArrays = 69,
    EXTBlendMinMax = 70,
    EXTClipControl = 71,
    EXTColorBufferFloat = 72,
    EXTColorBufferHalfFloat = 73,
    EXTConservativeDepth = 74,
    EXTDepthClamp = 75,
    EXTDisjointTimerQuery = 76,
    EXTDisjointTimerQueryWebGL2 = 77,
    EXTFloatBlend = 78,
    EXTFragDepth = 79,
    EXTPolygonOffsetClamp = 80,
    EXTRenderSnorm = 81,
    EXTShaderTextureLOD = 82,
    EXTTextureCompressionBPTC = 83,
    EXTTextureCompressionRGTC = 84,
    EXTTextureFilterAnisotropic = 85,
    EXTTextureMirrorClampToEdge = 86,
    EXTTextureNorm16 = 87,
    EXTsRGB = 88,
    KHRParallelShaderCompile = 89,
    NVShaderNoperspectiveInterpolation = 90,
    OESDrawBuffersIndexed = 91,
    OESElementIndexUint = 92,
    OESFBORenderMipmap = 93,
    OESSampleVariables = 94,
    OESShaderMultisampleInterpolation = 95,
    OESStandardDerivatives = 96,
    OESTextureFloat = 97,
    OESTextureFloatLinear = 98,
    OESTextureHalfFloat = 99,
    OESTextureHalfFloatLinear = 100,
    OESVertexArrayObject = 101,
};

} // namespace WebCore
