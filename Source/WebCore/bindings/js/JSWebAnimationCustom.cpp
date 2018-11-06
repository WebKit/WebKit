/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "JSWebAnimation.h"

#include "Document.h"
#include "JSAnimationEffect.h"
#include "JSAnimationTimeline.h"
#include "JSCSSAnimation.h"
#include "JSCSSTransition.h"
#include "JSDOMConstructor.h"

namespace WebCore {

using namespace JSC;

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<WebAnimation>&& value)
{
    if (value->isCSSAnimation())
        return createWrapper<CSSAnimation>(globalObject, WTFMove(value));
    if (value->isCSSTransition())
        return createWrapper<CSSTransition>(globalObject, WTFMove(value));
    return createWrapper<WebAnimation>(globalObject, WTFMove(value));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, WebAnimation& value)
{
    return wrap(state, globalObject, value);
}

EncodedJSValue JSC_HOST_CALL constructJSWebAnimation(ExecState& state)
{
    VM& vm = state.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    auto* jsConstructor = jsCast<JSDOMConstructorBase*>(state.jsCallee());
    ASSERT(jsConstructor);
    auto* context = jsConstructor->scriptExecutionContext();
    if (UNLIKELY(!context))
        return throwConstructorScriptExecutionContextUnavailableError(state, throwScope, "Animation");
    auto& document = downcast<Document>(*context);
    auto effect = convert<IDLNullable<IDLInterface<AnimationEffect>>>(state, state.argument(0), [](ExecState& state, ThrowScope& scope) {
        throwArgumentTypeError(state, scope, 0, "effect", "Animation", nullptr, "AnimationEffect");
    });
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    if (state.argument(1).isUndefined()) {
        auto object = WebAnimation::create(document, WTFMove(effect));
        return JSValue::encode(toJSNewlyCreated<IDLInterface<WebAnimation>>(state, *jsConstructor->globalObject(), WTFMove(object)));
    }

    auto timeline = convert<IDLNullable<IDLInterface<AnimationTimeline>>>(state, state.uncheckedArgument(1), [](ExecState& state, ThrowScope& scope) {
        throwArgumentTypeError(state, scope, 1, "timeline", "Animation", nullptr, "AnimationTimeline");
    });
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    auto object = WebAnimation::create(document, WTFMove(effect), WTFMove(timeline));
    return JSValue::encode(toJSNewlyCreated<IDLInterface<WebAnimation>>(state, *jsConstructor->globalObject(), WTFMove(object)));
}

} // namespace WebCore
