/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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
#include "JSDeprecatedCSSOMValue.h"

#include "JSCSSStyleDeclarationCustom.h"
#include "JSDeprecatedCSSOMPrimitiveValue.h"
#include "JSDeprecatedCSSOMValueList.h"
#include "JSNode.h"
#include "WebCoreOpaqueRootInlines.h"

namespace WebCore {
using namespace JSC;

bool JSDeprecatedCSSOMValueOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, const char** reason)
{
    JSDeprecatedCSSOMValue* jsCSSValue = jsCast<JSDeprecatedCSSOMValue*>(handle.slot()->asCell());
    if (!jsCSSValue->hasCustomProperties())
        return false;

    if (UNLIKELY(reason))
        *reason = "CSSStyleDeclaration is opaque root";

    return containsWebCoreOpaqueRoot(visitor, jsCSSValue->wrapped().owner());
}

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<DeprecatedCSSOMValue>&& value)
{
    if (value->isValueList())
        return createWrapper<DeprecatedCSSOMValueList>(globalObject, WTFMove(value));
    // Expose CSS-wide keywords as plain CSSValues to keep the existing behavior.
    if (auto* primitiveValue = dynamicDowncast<DeprecatedCSSOMPrimitiveValue>(value.get()); primitiveValue && !primitiveValue->isCSSWideKeyword())
        return createWrapper<DeprecatedCSSOMPrimitiveValue>(globalObject, WTFMove(value));
    return createWrapper<DeprecatedCSSOMValue>(globalObject, WTFMove(value));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, DeprecatedCSSOMValue& value)
{
    return wrap(lexicalGlobalObject, globalObject, value);
}

} // namespace WebCore
