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
#include "JSDOMConvert.h"

namespace WebCore {

using namespace JSC;

EncodedJSValue constructJSWebAnimation(JSGlobalObject* lexicalGlobalObject, CallFrame& callFrame)
{
    VM& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    auto* jsConstructor = jsCast<JSDOMConstructorBase*>(callFrame.jsCallee());
    ASSERT(jsConstructor);
    CheckedPtr context = jsConstructor->scriptExecutionContext();
    if (!context) [[unlikely]]
        return throwConstructorScriptExecutionContextUnavailableError(*lexicalGlobalObject, throwScope, "Animation"_s);

    CheckedRef document = downcast<Document>(*context);
    auto effect = convert<IDLNullable<IDLInterface<AnimationEffect>>>(*lexicalGlobalObject, callFrame.argument(0), [](JSGlobalObject& lexicalGlobalObject, ThrowScope& scope) {
        throwArgumentTypeError(lexicalGlobalObject, scope, 0, "effect"_s, "Animation"_s, nullptr, "AnimationEffect"_s);
    });
    if (effect.hasException(throwScope)) [[unlikely]]
        return encodedJSValue();

    if (callFrame.argument(1).isUndefined()) {
        auto object = WebAnimation::create(document, effect.releaseReturnValue());
        return JSValue::encode(toJSNewlyCreated<IDLInterface<WebAnimation>>(*lexicalGlobalObject, *jsConstructor->globalObject(), WTF::move(object)));
    }

    auto timeline = convert<IDLNullable<IDLInterface<AnimationTimeline>>>(*lexicalGlobalObject, callFrame.uncheckedArgument(1), [](JSGlobalObject& lexicalGlobalObject, ThrowScope& scope) {
        throwArgumentTypeError(lexicalGlobalObject, scope, 1, "timeline"_s, "Animation"_s, nullptr, "AnimationTimeline"_s);
    });
    if (timeline.hasException(throwScope)) [[unlikely]]
        return encodedJSValue();

    auto object = WebAnimation::create(document, effect.releaseReturnValue(), timeline.releaseReturnValue());
    return JSValue::encode(toJSNewlyCreated<IDLInterface<WebAnimation>>(*lexicalGlobalObject, *jsConstructor->globalObject(), WTF::move(object)));
}

} // namespace WebCore
