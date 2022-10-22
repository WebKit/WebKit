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

#include "JSCSSKeywordValue.h"
#include "JSCSSMathClamp.h"
#include "JSCSSMathInvert.h"
#include "JSCSSMathMax.h"
#include "JSCSSMathMin.h"
#include "JSCSSMathNegate.h"
#include "JSCSSMathProduct.h"
#include "JSCSSMathSum.h"
#include "JSCSSMathValue.h"
#include "JSCSSNumericValue.h"
#include "JSCSSStyleImageValue.h"
#include "JSCSSTransformValue.h"
#include "JSCSSUnitValue.h"
#include "JSCSSUnparsedValue.h"
#include "JSDOMWrapperCache.h"

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<CSSStyleValue>&& value)
{
    switch (value->getType()) {
    case CSSStyleValueType::CSSStyleImageValue:
        return createWrapper<CSSStyleImageValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathClamp:
        return createWrapper<CSSMathClamp>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathInvert:
        return createWrapper<CSSMathInvert>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathMin:
        return createWrapper<CSSMathMin>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathMax:
        return createWrapper<CSSMathMax>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathNegate:
        return createWrapper<CSSMathNegate>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathProduct:
        return createWrapper<CSSMathProduct>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSMathSum:
        return createWrapper<CSSMathSum>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSUnitValue:
        return createWrapper<CSSUnitValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSUnparsedValue:
        return createWrapper<CSSUnparsedValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSKeywordValue:
        return createWrapper<CSSKeywordValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSTransformValue:
        return createWrapper<CSSTransformValue>(globalObject, WTFMove(value));
    case CSSStyleValueType::CSSStyleValue:
        return createWrapper<CSSStyleValue>(globalObject, WTFMove(value));
    }

    ASSERT_NOT_REACHED();
    return createWrapper<CSSStyleValue>(globalObject, WTFMove(value));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, CSSStyleValue& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

} // namespace WebCore
