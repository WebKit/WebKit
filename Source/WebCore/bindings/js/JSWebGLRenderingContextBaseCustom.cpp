/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include "EXTBlendMinMax.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "ExceptionCode.h"
#include "JSANGLEInstancedArrays.h"
#include "JSEXTBlendMinMax.h"
#include "JSEXTFragDepth.h"
#include "JSEXTShaderTextureLOD.h"
#include "JSEXTTextureFilterAnisotropic.h"
#include "JSEXTsRGB.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSImageData.h"
#include "JSOESElementIndexUint.h"
#include "JSOESStandardDerivatives.h"
#include "JSOESTextureFloat.h"
#include "JSOESTextureFloatLinear.h"
#include "JSOESTextureHalfFloat.h"
#include "JSOESTextureHalfFloatLinear.h"
#include "JSOESVertexArrayObject.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLCompressedTextureATC.h"
#include "JSWebGLCompressedTexturePVRTC.h"
#include "JSWebGLCompressedTextureS3TC.h"
#include "JSWebGLDebugRendererInfo.h"
#include "JSWebGLDebugShaders.h"
#include "JSWebGLDepthTexture.h"
#include "JSWebGLDrawBuffers.h"
#include "JSWebGLFramebuffer.h"
#include "JSWebGLLoseContext.h"
#include "JSWebGLProgram.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLRenderingContext.h"
#include "JSWebGLShader.h"
#include "JSWebGLTexture.h"
#include "JSWebGLUniformLocation.h"
#include "JSWebGLVertexArrayObject.h"
#include "JSWebGLVertexArrayObjectOES.h"
#include "JSWebKitCSSMatrix.h"
#include "OESElementIndexUint.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "WebGLBuffer.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLExtension.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include <runtime/Error.h>
#include <runtime/JSObjectInlines.h>
#include <runtime/JSTypedArrays.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/TypedArrays.h>
#include <wtf/FastMalloc.h>

#if ENABLE(VIDEO)
#include "JSHTMLVideoElement.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#endif

using namespace JSC;

namespace WebCore {

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<WebGLRenderingContextBase>&& object)
{
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(object))
        return createWrapper<WebGL2RenderingContext>(globalObject, WTFMove(object));
#endif
    return createWrapper<WebGLRenderingContext>(globalObject, WTFMove(object));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, WebGLRenderingContextBase& object)
{
    return wrap(state, globalObject, object);
}
    
static JSValue toJS(ExecState& state, JSDOMGlobalObject& globalObject, WebGLExtension* extension)
{
    if (!extension)
        return jsNull();
    switch (extension->getName()) {
    case WebGLExtension::WebGLLoseContextName:
        return toJS(&state, &globalObject, static_cast<WebGLLoseContext*>(extension));
    case WebGLExtension::EXTShaderTextureLODName:
        return toJS(&state, &globalObject, static_cast<EXTShaderTextureLOD*>(extension));
    case WebGLExtension::EXTTextureFilterAnisotropicName:
        return toJS(&state, &globalObject, static_cast<EXTTextureFilterAnisotropic*>(extension));
    case WebGLExtension::EXTsRGBName:
        return toJS(&state, &globalObject, static_cast<EXTsRGB*>(extension));
    case WebGLExtension::EXTFragDepthName:
        return toJS(&state, &globalObject, static_cast<EXTFragDepth*>(extension));
    case WebGLExtension::EXTBlendMinMaxName:
        return toJS(&state, &globalObject, static_cast<EXTBlendMinMax*>(extension));
    case WebGLExtension::OESStandardDerivativesName:
        return toJS(&state, &globalObject, static_cast<OESStandardDerivatives*>(extension));
    case WebGLExtension::OESTextureFloatName:
        return toJS(&state, &globalObject, static_cast<OESTextureFloat*>(extension));
    case WebGLExtension::OESTextureFloatLinearName:
        return toJS(&state, &globalObject, static_cast<OESTextureFloatLinear*>(extension));
    case WebGLExtension::OESTextureHalfFloatName:
        return toJS(&state, &globalObject, static_cast<OESTextureHalfFloat*>(extension));
    case WebGLExtension::OESTextureHalfFloatLinearName:
        return toJS(&state, &globalObject, static_cast<OESTextureHalfFloatLinear*>(extension));
    case WebGLExtension::OESVertexArrayObjectName:
        return toJS(&state, &globalObject, static_cast<OESVertexArrayObject*>(extension));
    case WebGLExtension::OESElementIndexUintName:
        return toJS(&state, &globalObject, static_cast<OESElementIndexUint*>(extension));
    case WebGLExtension::WebGLDebugRendererInfoName:
        return toJS(&state, &globalObject, static_cast<WebGLDebugRendererInfo*>(extension));
    case WebGLExtension::WebGLDebugShadersName:
        return toJS(&state, &globalObject, static_cast<WebGLDebugShaders*>(extension));
    case WebGLExtension::WebGLCompressedTextureATCName:
        return toJS(&state, &globalObject, static_cast<WebGLCompressedTextureATC*>(extension));
    case WebGLExtension::WebGLCompressedTexturePVRTCName:
        return toJS(&state, &globalObject, static_cast<WebGLCompressedTexturePVRTC*>(extension));
    case WebGLExtension::WebGLCompressedTextureS3TCName:
        return toJS(&state, &globalObject, static_cast<WebGLCompressedTextureS3TC*>(extension));
    case WebGLExtension::WebGLDepthTextureName:
        return toJS(&state, &globalObject, static_cast<WebGLDepthTexture*>(extension));
    case WebGLExtension::WebGLDrawBuffersName:
        return toJS(&state, &globalObject, static_cast<WebGLDrawBuffers*>(extension));
    case WebGLExtension::ANGLEInstancedArraysName:
        return toJS(&state, &globalObject, static_cast<ANGLEInstancedArrays*>(extension));
    }
    ASSERT_NOT_REACHED();
    return jsNull();
}

bool JSWebGLRenderingContextBaseOwner::isReachableFromOpaqueRoots(Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    JSWebGLRenderingContextBase* jsWebGLRenderingContext = jsCast<JSWebGLRenderingContextBase*>(handle.slot()->asCell());
    void* root = WebCore::root(jsWebGLRenderingContext->wrapped().canvas());
    return visitor.containsOpaqueRoot(root);
}

void JSWebGLRenderingContextBase::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(&wrapped());
    visitor.addOpaqueRoot(root(wrapped().canvas()));
}

JSValue JSWebGLRenderingContextBase::getExtension(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    auto name = state.uncheckedArgument(0).toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *globalObject(), wrapped().getExtension(name));
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
