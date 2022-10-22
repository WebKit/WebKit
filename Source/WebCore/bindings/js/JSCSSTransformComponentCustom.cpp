/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "JSCSSTransformComponent.h"

#include "JSCSSMatrixComponent.h"
#include "JSCSSPerspective.h"
#include "JSCSSRotate.h"
#include "JSCSSScale.h"
#include "JSCSSSkew.h"
#include "JSCSSSkewX.h"
#include "JSCSSSkewY.h"
#include "JSCSSTranslate.h"
#include "JSDOMWrapperCache.h"

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<CSSTransformComponent>&& value)
{
    switch (value->getType()) {
    case CSSTransformType::MatrixComponent:
        return createWrapper<CSSMatrixComponent>(globalObject, WTFMove(value));
    case CSSTransformType::Perspective:
        return createWrapper<CSSPerspective>(globalObject, WTFMove(value));
    case CSSTransformType::Rotate:
        return createWrapper<CSSRotate>(globalObject, WTFMove(value));
    case CSSTransformType::Scale:
        return createWrapper<CSSScale>(globalObject, WTFMove(value));
    case CSSTransformType::Skew:
        return createWrapper<CSSSkew>(globalObject, WTFMove(value));
    case CSSTransformType::SkewX:
        return createWrapper<CSSSkewX>(globalObject, WTFMove(value));
    case CSSTransformType::SkewY:
        return createWrapper<CSSSkewY>(globalObject, WTFMove(value));
    case CSSTransformType::Translate:
        return createWrapper<CSSTranslate>(globalObject, WTFMove(value));
    }
    ASSERT_NOT_REACHED();
    return createWrapper<CSSTransformComponent>(globalObject, WTFMove(value));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, CSSTransformComponent& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

} // namespace WebCore
