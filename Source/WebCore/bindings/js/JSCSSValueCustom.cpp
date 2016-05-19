/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSCSSValue.h"

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "JSCSSPrimitiveValue.h"
#include "JSCSSValueList.h"
#include "JSNode.h"
#include "JSSVGColor.h"
#include "JSSVGPaint.h"
#include "JSWebKitCSSFilterValue.h"
#include "JSWebKitCSSTransformValue.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSTransformValue.h"

using namespace JSC;

namespace WebCore {

bool JSCSSValueOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void* context, SlotVisitor& visitor)
{
    JSCSSValue* jsCSSValue = jsCast<JSCSSValue*>(handle.slot()->asCell());
    if (!jsCSSValue->hasCustomProperties())
        return false;
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    void* root = world->m_cssValueRoots.get(&jsCSSValue->wrapped());
    if (!root)
        return false;
    return visitor.containsOpaqueRoot(root);
}

void JSCSSValueOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSCSSValue* jsCSSValue = jsCast<JSCSSValue*>(handle.slot()->asCell());
    DOMWrapperWorld& world = *static_cast<DOMWrapperWorld*>(context);
    world.m_cssValueRoots.remove(&jsCSSValue->wrapped());
    uncacheWrapper(world, &jsCSSValue->wrapped(), jsCSSValue);
}

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<CSSValue>&& value)
{
    if (value->isWebKitCSSTransformValue())
        return CREATE_DOM_WRAPPER(globalObject, WebKitCSSTransformValue, WTFMove(value));
    if (value->isWebKitCSSFilterValue())
        return CREATE_DOM_WRAPPER(globalObject, WebKitCSSFilterValue, WTFMove(value));
    if (value->isValueList())
        return CREATE_DOM_WRAPPER(globalObject, CSSValueList, WTFMove(value));
    if (value->isSVGPaint())
        return CREATE_DOM_WRAPPER(globalObject, SVGPaint, WTFMove(value));
    if (value->isSVGColor())
        return CREATE_DOM_WRAPPER(globalObject, SVGColor, WTFMove(value));
    if (value->isPrimitiveValue())
        return CREATE_DOM_WRAPPER(globalObject, CSSPrimitiveValue, WTFMove(value));
    return CREATE_DOM_WRAPPER(globalObject, CSSValue, WTFMove(value));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, CSSValue& value)
{
    // Scripts should only ever see cloned CSSValues, never the internal ones.
    ASSERT(value.isCSSOMSafe());

    // If we're here under erroneous circumstances, prefer returning null over a potentially insecure value.
    if (!value.isCSSOMSafe())
        return jsNull();

    return wrap(state, globalObject, value);
}

} // namespace WebCore
