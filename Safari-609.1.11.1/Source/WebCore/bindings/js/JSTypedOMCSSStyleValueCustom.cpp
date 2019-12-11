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
#include "JSTypedOMCSSStyleValue.h"

#if ENABLE(CSS_TYPED_OM)

#include "JSDOMWrapperCache.h"
#include "JSTypedOMCSSImageValue.h"
#include "JSTypedOMCSSUnitValue.h"
#include "JSTypedOMCSSUnparsedValue.h"

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<TypedOMCSSStyleValue>&& value)
{
    if (value->isUnitValue())
        return createWrapper<TypedOMCSSUnitValue>(globalObject, WTFMove(value));
    if (value->isUnparsedValue())
        return createWrapper<TypedOMCSSUnparsedValue>(globalObject, WTFMove(value));
    if (value->isImageValue())
        return createWrapper<TypedOMCSSImageValue>(globalObject, WTFMove(value));

    ASSERT_NOT_REACHED();
    return createWrapper<TypedOMCSSStyleValue>(globalObject, WTFMove(value));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, TypedOMCSSStyleValue& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

} // namespace WebCore

#endif
