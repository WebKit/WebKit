/*
 * Copyright (C) 2007, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "JSHTMLCanvasElement.h"

#include "CanvasContextAttributes.h"
#include "HTMLCanvasElement.h"
#include "JSCanvasRenderingContext2D.h"
#include <bindings/ScriptObject.h>
#include <wtf/GetPtr.h>

#if ENABLE(WEBGL)
#include "JSDictionary.h"
#include "JSWebGLRenderingContextBase.h"
#include "WebGLContextAttributes.h"
#endif

using namespace JSC;

namespace WebCore {

#if ENABLE(WEBGL)

static RefPtr<CanvasContextAttributes> attributesFor3DContext(ExecState& state, ThrowScope& scope)
{
    JSValue initializerValue = state.argument(1);
    if (initializerValue.isUndefinedOrNull())
        return nullptr;
    
    JSObject* initializerObject = initializerValue.toObject(&state);
    ASSERT_UNUSED(scope, !scope.exception());
    JSDictionary dictionary(&state, initializerObject);
    
    GraphicsContext3D::Attributes graphicsAttrs;
    
    dictionary.tryGetProperty("alpha", graphicsAttrs.alpha);
    dictionary.tryGetProperty("depth", graphicsAttrs.depth);
    dictionary.tryGetProperty("stencil", graphicsAttrs.stencil);
    dictionary.tryGetProperty("antialias", graphicsAttrs.antialias);
    dictionary.tryGetProperty("premultipliedAlpha", graphicsAttrs.premultipliedAlpha);
    dictionary.tryGetProperty("preserveDrawingBuffer", graphicsAttrs.preserveDrawingBuffer);
    dictionary.tryGetProperty("preferLowPowerToHighPerformance", graphicsAttrs.preferLowPowerToHighPerformance);

    return WebGLContextAttributes::create(graphicsAttrs);
}

#endif

JSValue JSHTMLCanvasElement::getContext(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    HTMLCanvasElement& canvas = wrapped();
    const String& contextId = state.uncheckedArgument(0).toWTFString(&state);
    
    RefPtr<CanvasContextAttributes> attributes;

#if ENABLE(WEBGL)
    if (HTMLCanvasElement::is3dType(contextId)) {
        attributes = attributesFor3DContext(state, scope);
        RETURN_IF_EXCEPTION(scope, JSValue());
    }
#endif
    
    auto* context = canvas.getContext(contextId, attributes.get());
    if (!context)
        return jsNull();

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(*context))
        return toJS(&state, globalObject(), downcast<WebGLRenderingContextBase>(*context));
#endif

    return toJS(&state, globalObject(), downcast<CanvasRenderingContext2D>(*context));
}

JSValue JSHTMLCanvasElement::toDataURL(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto type = convert<IDLNullable<IDLDOMString>>(state, state.argument(0));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    Optional<double> quality;
    auto qualityValue = state.argument(1);
    if (qualityValue.isNumber())
        quality = qualityValue.toNumber(&state);

    // We would use toJS<IDLString> here, but it uses jsStringWithCache and we historically
    // did not cache here, presumably because results are likely to be differing long strings.
    auto result = wrapped().toDataURL(type, quality);
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }
    return jsString(&state, result.releaseReturnValue());
}

} // namespace WebCore
