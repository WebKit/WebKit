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

#include "JSANGLEInstancedArrays.h"
#include "JSDOMConvertBufferSource.h"
#include "JSEXTBlendFuncExtended.h"
#include "JSEXTBlendMinMax.h"
#include "JSEXTClipControl.h"
#include "JSEXTColorBufferFloat.h"
#include "JSEXTColorBufferHalfFloat.h"
#include "JSEXTConservativeDepth.h"
#include "JSEXTDepthClamp.h"
#include "JSEXTDisjointTimerQuery.h"
#include "JSEXTDisjointTimerQueryWebGL2.h"
#include "JSEXTFloatBlend.h"
#include "JSEXTFragDepth.h"
#include "JSEXTPolygonOffsetClamp.h"
#include "JSEXTRenderSnorm.h"
#include "JSEXTShaderTextureLOD.h"
#include "JSEXTTextureCompressionBPTC.h"
#include "JSEXTTextureCompressionRGTC.h"
#include "JSEXTTextureFilterAnisotropic.h"
#include "JSEXTTextureMirrorClampToEdge.h"
#include "JSEXTTextureNorm16.h"
#include "JSEXTsRGB.h"
#include "JSKHRParallelShaderCompile.h"
#include "JSNVShaderNoperspectiveInterpolation.h"
#include "JSOESDrawBuffersIndexed.h"
#include "JSOESElementIndexUint.h"
#include "JSOESFBORenderMipmap.h"
#include "JSOESSampleVariables.h"
#include "JSOESShaderMultisampleInterpolation.h"
#include "JSOESStandardDerivatives.h"
#include "JSOESTextureFloat.h"
#include "JSOESTextureFloatLinear.h"
#include "JSOESTextureHalfFloat.h"
#include "JSOESTextureHalfFloatLinear.h"
#include "JSOESVertexArrayObject.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLClipCullDistance.h"
#include "JSWebGLColorBufferFloat.h"
#include "JSWebGLCompressedTextureASTC.h"
#include "JSWebGLCompressedTextureETC.h"
#include "JSWebGLCompressedTextureETC1.h"
#include "JSWebGLCompressedTexturePVRTC.h"
#include "JSWebGLCompressedTextureS3TC.h"
#include "JSWebGLCompressedTextureS3TCsRGB.h"
#include "JSWebGLDebugRendererInfo.h"
#include "JSWebGLDebugShaders.h"
#include "JSWebGLDepthTexture.h"
#include "JSWebGLDrawBuffers.h"
#include "JSWebGLDrawInstancedBaseVertexBaseInstance.h"
#include "JSWebGLFramebuffer.h"
#include "JSWebGLLoseContext.h"
#include "JSWebGLMultiDraw.h"
#include "JSWebGLMultiDrawInstancedBaseVertexBaseInstance.h"
#include "JSWebGLPolygonMode.h"
#include "JSWebGLProgram.h"
#include "JSWebGLProvokingVertex.h"
#include "JSWebGLQuery.h"
#include "JSWebGLRenderSharedExponent.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLSampler.h"
#include "JSWebGLStencilTexturing.h"
#include "JSWebGLTexture.h"
#include "JSWebGLTimerQueryEXT.h"
#include "JSWebGLTransformFeedback.h"
#include "JSWebGLVertexArrayObject.h"
#include "JSWebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/JSCInlines.h>

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
            for (auto& value : values)
                list.append(jsBoolean(value));
            RELEASE_ASSERT(!list.hasOverflowed());
            return constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);
        }, [&] (const Vector<int>& values) -> JSValue {
            MarkedArgumentBuffer list;
            for (auto& value : values)
                list.append(jsNumber(value));
            RELEASE_ASSERT(!list.hasOverflowed());
            return constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);
        }, [&] (const Vector<unsigned>& values) -> JSValue {
            MarkedArgumentBuffer list;
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
    return WTF::switchOn(extensionAny,
        [&] (Ref<ANGLEInstancedArrays> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTBlendFuncExtended> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTBlendMinMax> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTClipControl> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTColorBufferFloat> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTColorBufferHalfFloat> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTConservativeDepth> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTDepthClamp> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTDisjointTimerQuery> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTDisjointTimerQueryWebGL2> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTFloatBlend> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTFragDepth> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTPolygonOffsetClamp> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTRenderSnorm> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTShaderTextureLOD> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTTextureCompressionBPTC> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTTextureCompressionRGTC> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTTextureFilterAnisotropic> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTTextureMirrorClampToEdge> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTTextureNorm16> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<EXTsRGB> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<KHRParallelShaderCompile> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<NVShaderNoperspectiveInterpolation> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESDrawBuffersIndexed> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESElementIndexUint> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESFBORenderMipmap> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESSampleVariables> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESShaderMultisampleInterpolation> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESStandardDerivatives> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESTextureFloat> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESTextureFloatLinear> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESTextureHalfFloat> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESTextureHalfFloatLinear> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<OESVertexArrayObject> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLClipCullDistance> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLColorBufferFloat> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLCompressedTextureASTC> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLCompressedTextureETC> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLCompressedTextureETC1> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLCompressedTexturePVRTC> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLCompressedTextureS3TC> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLCompressedTextureS3TCsRGB> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLDebugRendererInfo> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLDebugShaders> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLDepthTexture> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLDrawBuffers> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLDrawInstancedBaseVertexBaseInstance> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLLoseContext> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLMultiDraw> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLMultiDrawInstancedBaseVertexBaseInstance> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLPolygonMode> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLProvokingVertex> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLRenderSharedExponent> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        },
        [&] (Ref<WebGLStencilTexturing> extension) {
            return toJS(&lexicalGlobalObject, &globalObject, WTFMove(extension));
        });
}

}

#endif

