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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "JSWebKitCSSTransformValue.h"
#include "WebKitCSSTransformValue.h"

#if ENABLE(CSS_FILTERS)
#include "JSWebKitCSSFilterValue.h"
#include "WebKitCSSFilterValue.h"
#endif

#if ENABLE(SVG)
#include "JSSVGColor.h"
#include "JSSVGPaint.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#endif

using namespace JSC;

namespace WebCore {

bool JSCSSValueOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void* context, SlotVisitor& visitor)
{
    JSCSSValue* jsCSSValue = static_cast<JSCSSValue*>(handle.get().asCell());
    if (!jsCSSValue->hasCustomProperties())
        return false;
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    void* root = world->m_cssValueRoots.get(jsCSSValue->impl());
    if (!root)
        return false;
    return visitor.containsOpaqueRoot(root);
}

void JSCSSValueOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    JSCSSValue* jsCSSValue = static_cast<JSCSSValue*>(handle.get().asCell());
    DOMWrapperWorld* world = static_cast<DOMWrapperWorld*>(context);
    world->m_cssValueRoots.remove(jsCSSValue->impl());
    uncacheWrapper(world, jsCSSValue->impl(), jsCSSValue);
    jsCSSValue->releaseImpl();
}

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, CSSValue* value)
{
    if (!value)
        return jsNull();

    if (value->isWebKitCSSTransformValue())
        return JSWebKitCSSTransformValue::create(getDOMStructure<JSWebKitCSSTransformValue>(exec, globalObject), globalObject, static_cast<WebKitCSSTransformValue*>(value));
#if ENABLE(CSS_FILTERS)
    if (value->isWebKitCSSFilterValue())
        return JSWebKitCSSFilterValue::create(getDOMStructure<JSWebKitCSSFilterValue>(exec, globalObject), globalObject, static_cast<WebKitCSSFilterValue*>(value));
#endif
    if (value->isValueList())
        return JSCSSValueList::create(getDOMStructure<JSCSSValueList>(exec, globalObject), globalObject, static_cast<CSSValueList*>(value));
#if ENABLE(SVG)
    if (value->isSVGPaint())
        return JSSVGPaint::create(getDOMStructure<JSSVGPaint>(exec, globalObject), globalObject, static_cast<SVGPaint*>(value));
    if (value->isSVGColor())
        return JSSVGColor::create(getDOMStructure<JSSVGColor>(exec, globalObject), globalObject, static_cast<SVGColor*>(value));
#endif
    if (value->isPrimitiveValue())
        return JSCSSPrimitiveValue::create(getDOMStructure<JSCSSPrimitiveValue>(exec, globalObject), globalObject, static_cast<CSSPrimitiveValue*>(value));

    return JSCSSValue::create(getDOMStructure<JSCSSValue>(exec, globalObject), globalObject, value);
}

} // namespace WebCore
