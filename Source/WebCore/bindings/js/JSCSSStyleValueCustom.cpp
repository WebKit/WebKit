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
#include "JSCSSStyleValue.h"

#if ENABLE(CSS_TYPED_OM)

#include "JSCSSStyleImageValue.h"
#include "JSCSSUnitValue.h"
#include "JSCSSUnparsedValue.h"
#include "JSDOMWrapperCache.h"

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<CSSStyleValue>&& value)
{
    switch (value->getType()) {
    case CSSStyleValueType::CSSUnitValue:
        return createWrapper<CSSUnitValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSUnparsedValue:
        return createWrapper<CSSUnparsedValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSStyleImageValue:
        return createWrapper<CSSStyleImageValue>(globalObject, WTFMove(value));
    default:
        return createWrapper<CSSStyleValue>(globalObject, WTFMove(value));
    }

}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, CSSStyleValue& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

} // namespace WebCore

#endif
