/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSDOMConvertWebGL.h"

#if ENABLE(WEBGL)

#include "ANGLEInstancedArrays.h"
#include "EXTBlendMinMax.h"
#include "EXTClipControl.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTConservativeDepth.h"
#include "EXTDepthClamp.h"
#include "EXTDisjointTimerQuery.h"
#include "EXTDisjointTimerQueryWebGL2.h"
#include "EXTFloatBlend.h"
#include "EXTFragDepth.h"
#include "EXTPolygonOffsetClamp.h"
#include "EXTRenderSnorm.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureCompressionBPTC.h"
#include "EXTTextureCompressionRGTC.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTTextureMirrorClampToEdge.h"
#include "EXTTextureNorm16.h"
#include "EXTsRGB.h"
#include "JSANGLEInstancedArraysInlines.h"
#include "JSDOMConvertBufferSource.h"
#include "JSEXTBlendMinMaxInlines.h"
#include "JSEXTClipControlInlines.h"
#include "JSEXTColorBufferFloatInlines.h"
#include "JSEXTColorBufferHalfFloatInlines.h"
#include "JSEXTConservativeDepthInlines.h"
#include "JSEXTDepthClampInlines.h"
#include "JSEXTDisjointTimerQueryInlines.h"
#include "JSEXTDisjointTimerQueryWebGL2Inlines.h"
#include "JSEXTFloatBlendInlines.h"
#include "JSEXTFragDepthInlines.h"
#include "JSEXTPolygonOffsetClampInlines.h"
#include "JSEXTRenderSnormInlines.h"
#include "JSEXTShaderTextureLODInlines.h"
#include "JSEXTTextureCompressionBPTCInlines.h"
#include "JSEXTTextureCompressionRGTCInlines.h"
#include "JSEXTTextureFilterAnisotropicInlines.h"
#include "JSEXTTextureMirrorClampToEdgeInlines.h"
#include "JSEXTTextureNorm16Inlines.h"
#include "JSEXTsRGBInlines.h"
#include "JSKHRParallelShaderCompileInlines.h"
#include "JSNVShaderNoperspectiveInterpolationInlines.h"
#include "JSOESDrawBuffersIndexedInlines.h"
#include "JSOESElementIndexUintInlines.h"
#include "JSOESFBORenderMipmapInlines.h"
#include "JSOESSampleVariablesInlines.h"
#include "JSOESShaderMultisampleInterpolationInlines.h"
#include "JSOESStandardDerivativesInlines.h"
#include "JSOESTextureFloatInlines.h"
#include "JSOESTextureFloatLinearInlines.h"
#include "JSOESTextureHalfFloatInlines.h"
#include "JSOESTextureHalfFloatLinearInlines.h"
#include "JSOESVertexArrayObjectInlines.h"
#include "JSWebGLBlendFuncExtendedInlines.h"
#include "JSWebGLBufferInlines.h"
#include "JSWebGLClipCullDistanceInlines.h"
#include "JSWebGLColorBufferFloatInlines.h"
#include "JSWebGLCompressedTextureASTCInlines.h"
#include "JSWebGLCompressedTextureETCInlines.h"
#include "JSWebGLCompressedTextureETC1Inlines.h"
#include "JSWebGLCompressedTexturePVRTCInlines.h"
#include "JSWebGLCompressedTextureS3TCInlines.h"
#include "JSWebGLCompressedTextureS3TCsRGBInlines.h"
#include "JSWebGLDebugRendererInfoInlines.h"
#include "JSWebGLDebugShadersInlines.h"
#include "JSWebGLDepthTextureInlines.h"
#include "JSWebGLDrawBuffersInlines.h"
#include "JSWebGLDrawInstancedBaseVertexBaseInstanceInlines.h"
#include "JSWebGLFramebufferInlines.h"
#include "JSWebGLLoseContextInlines.h"
#include "JSWebGLMultiDrawInlines.h"
#include "JSWebGLMultiDrawInstancedBaseVertexBaseInstanceInlines.h"
#include "JSWebGLPolygonModeInlines.h"
#include "JSWebGLProgramInlines.h"
#include "JSWebGLProvokingVertexInlines.h"
#include "JSWebGLQueryInlines.h"
#include "JSWebGLRenderSharedExponentInlines.h"
#include "JSWebGLRenderbufferInlines.h"
#include "JSWebGLSamplerInlines.h"
#include "JSWebGLStencilTexturingInlines.h"
#include "JSWebGLTextureInlines.h"
#include "JSWebGLTimerQueryEXTInlines.h"
#include "JSWebGLTransformFeedbackInlines.h"
#include "JSWebGLVertexArrayObjectInlines.h"
#include "JSWebGLVertexArrayObjectOESInlines.h"
#include "KHRParallelShaderCompile.h"
#include "NVShaderNoperspectiveInterpolation.h"
#include "OESDrawBuffersIndexed.h"
#include "OESElementIndexUint.h"
#include "OESFBORenderMipmap.h"
#include "OESSampleVariables.h"
#include "OESShaderMultisampleInterpolation.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "WebGLBlendFuncExtended.h"
#include "WebGLClipCullDistance.h"
#include "WebGLColorBufferFloat.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLCompressedTextureS3TCsRGB.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLLoseContext.h"
#include "WebGLMultiDraw.h"
#include "WebGLMultiDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLPolygonMode.h"
#include "WebGLProvokingVertex.h"
#include "WebGLRenderSharedExponent.h"
#include "WebGLStencilTexturing.h"

namespace WebCore {
using namespace JSC;

// FIXME: This should use the IDLUnion JSConverter.
JSValue convertToJSValue(JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, const WebGLAny& any)
{
    return WTF::switchOn(any,
        [] (std::nullptr_t) -> JSValue {
            return jsNull();
        }, [] (bool value) -> JSValue {
            return jsBoolean(value);
        }, [] (int value) -> JSValue {
            return jsNumber(value);
        }, [] (unsigned value) -> JSValue {
            return jsNumber(value);
        }, [] (long long value) -> JSValue {
            return jsNumber(value);
        }, [] (unsigned long long value) -> JSValue {
            return jsNumber(value);
        }, [] (float value) -> JSValue {
            return jsNumber(purifyNaN(value));
        }, [&] (const String& value) -> JSValue {
            return jsStringWithCache(lexicalGlobalObject.vm(), value);
        }, [&] (const Vector<bool>& values) -> JSValue {
            MarkedArgumentBuffer list;
            list.ensureCapacity(values.size());
            for (auto& value : values)
                list.append(jsBoolean(value));
            RELEASE_ASSERT(!list.hasOverflowed());
            return constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);
        }, [&] (const Vector<int>& values) -> JSValue {
            MarkedArgumentBuffer list;
            list.ensureCapacity(values.size());
            for (auto& value : values)
                list.append(jsNumber(value));
            RELEASE_ASSERT(!list.hasOverflowed());
            return constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);
        }, [&] (const Vector<unsigned>& values) -> JSValue {
            MarkedArgumentBuffer list;
            list.ensureCapacity(values.size());
            for (auto& value : values)
                list.append(jsNumber(value));
            RELEASE_ASSERT(!list.hasOverflowed());
            return constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);
        }, [&] (const RefPtr<Float32Array>& array) {
            return toJS(&lexicalGlobalObject, &globalObject, array.get());
        },
        [&] (const RefPtr<Int32Array>& array) {
            return toJS(&lexicalGlobalObject, &globalObject, array.get());
        },
        [&] (const RefPtr<Uint8Array>& array) {
            return toJS(&lexicalGlobalObject, &globalObject, array.get());
        },
        [&] (const RefPtr<Uint32Array>& array) {
            return toJS(&lexicalGlobalObject, &globalObject, array.get());
        },
        [&] (const RefPtr<WebGLBuffer>& buffer) {
            return toJS(&lexicalGlobalObject, &globalObject, buffer.get());
        },
        [&] (const RefPtr<WebGLFramebuffer>& buffer) {
            return toJS(&lexicalGlobalObject, &globalObject, buffer.get());
        },
        [&] (const RefPtr<WebGLProgram>& program) {
            return toJS(&lexicalGlobalObject, &globalObject, program.get());
        },
        [&] (const RefPtr<WebGLRenderbuffer>& buffer) {
            return toJS(&lexicalGlobalObject, &globalObject, buffer.get());
        },
        [&] (const RefPtr<WebGLTexture>& texture) {
            return toJS(&lexicalGlobalObject, &globalObject, texture.get());
        },
        [&] (const RefPtr<WebGLTimerQueryEXT>& query) {
            return toJS(&lexicalGlobalObject, &globalObject, query.get());
        },
        [&] (const RefPtr<WebGLVertexArrayObjectOES>& array) {
            return toJS(&lexicalGlobalObject, &globalObject, array.get());
        },
        [&] (const RefPtr<WebGLQuery>& query) {
            return toJS(&lexicalGlobalObject, &globalObject, query.get());
        },
        [&] (const RefPtr<WebGLSampler>& sampler) {
            return toJS(&lexicalGlobalObject, &globalObject, sampler.get());
        },
        [&] (const RefPtr<WebGLTransformFeedback>& transformFeedback) {
            return toJS(&lexicalGlobalObject, &globalObject, transformFeedback.get());
        },
        [&] (const RefPtr<WebGLVertexArrayObject>& array) {
            return toJS(&lexicalGlobalObject, &globalObject, array.get());
        }
    );
}

JSValue convertToJSValue(JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, WebGLExtensionAny extensionAny)
{
#define TO_JS(EXT) \
    case WebGLExtensionName::EXT: \
        return toJS(&lexicalGlobalObject, &globalObject, static_cast<EXT&>(extensionAny.get()));
    switch (extensionAny->name()) {
        TO_JS(ANGLEInstancedArrays);
        TO_JS(EXTBlendMinMax);
        TO_JS(EXTClipControl);
        TO_JS(EXTColorBufferFloat);
        TO_JS(EXTColorBufferHalfFloat);
        TO_JS(EXTConservativeDepth);
        TO_JS(EXTDepthClamp);
        TO_JS(EXTDisjointTimerQuery);
        TO_JS(EXTDisjointTimerQueryWebGL2);
        TO_JS(EXTFloatBlend);
        TO_JS(EXTFragDepth);
        TO_JS(EXTPolygonOffsetClamp);
        TO_JS(EXTRenderSnorm);
        TO_JS(EXTShaderTextureLOD);
        TO_JS(EXTTextureCompressionBPTC);
        TO_JS(EXTTextureCompressionRGTC);
        TO_JS(EXTTextureFilterAnisotropic);
        TO_JS(EXTTextureMirrorClampToEdge);
        TO_JS(EXTTextureNorm16);
        TO_JS(EXTsRGB);
        TO_JS(KHRParallelShaderCompile);
        TO_JS(NVShaderNoperspectiveInterpolation);
        TO_JS(OESDrawBuffersIndexed);
        TO_JS(OESElementIndexUint);
        TO_JS(OESFBORenderMipmap);
        TO_JS(OESSampleVariables);
        TO_JS(OESShaderMultisampleInterpolation);
        TO_JS(OESStandardDerivatives);
        TO_JS(OESTextureFloat);
        TO_JS(OESTextureFloatLinear);
        TO_JS(OESTextureHalfFloat);
        TO_JS(OESTextureHalfFloatLinear);
        TO_JS(OESVertexArrayObject);
        TO_JS(WebGLBlendFuncExtended);
        TO_JS(WebGLClipCullDistance);
        TO_JS(WebGLColorBufferFloat);
        TO_JS(WebGLCompressedTextureASTC);
        TO_JS(WebGLCompressedTextureETC);
        TO_JS(WebGLCompressedTextureETC1);
        TO_JS(WebGLCompressedTexturePVRTC);
        TO_JS(WebGLCompressedTextureS3TC);
        TO_JS(WebGLCompressedTextureS3TCsRGB);
        TO_JS(WebGLDebugRendererInfo);
        TO_JS(WebGLDebugShaders);
        TO_JS(WebGLDepthTexture);
        TO_JS(WebGLDrawBuffers);
        TO_JS(WebGLDrawInstancedBaseVertexBaseInstance);
        TO_JS(WebGLLoseContext);
        TO_JS(WebGLMultiDraw);
        TO_JS(WebGLMultiDrawInstancedBaseVertexBaseInstance);
        TO_JS(WebGLPolygonMode);
        TO_JS(WebGLProvokingVertex);
        TO_JS(WebGLRenderSharedExponent);
        TO_JS(WebGLStencilTexturing);
    }
    ASSERT_NOT_REACHED();
    return jsNull();
}

}

#endif

