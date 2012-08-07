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
#include "WebKitCSSTransformValue.h"

#include "CSSValueList.h"
#include "MemoryInstrumentation.h"
#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// These names must be kept in sync with TransformOperationType.
const char* const transformName[] = {
     0,
     "translate",
     "translateX",
     "translateY",
     "rotate",
     "scale",
     "scaleX",
     "scaleY",
     "skew",
     "skewX",
     "skewY",
     "matrix",
     "translateZ",
     "translate3d",
     "rotateX",
     "rotateY",
     "rotateZ",
     "rotate3d",
     "scaleZ",
     "scale3d",
     "perspective",
     "matrix3d"
};

WebKitCSSTransformValue::WebKitCSSTransformValue(TransformOperationType op)
    : CSSValueList(WebKitCSSTransformClass, CommaSeparator)
    , m_type(op)
{
}

String WebKitCSSTransformValue::customCssText() const
{
    StringBuilder result;
    if (m_type != UnknownTransformOperation) {
        ASSERT(static_cast<size_t>(m_type) < WTF_ARRAY_LENGTH(transformName));
        result.append(transformName[m_type]);
        result.append('(');
        result.append(CSSValueList::customCssText());
        result.append(')');
    }
    return result.toString();
}

#if ENABLE(CSS_VARIABLES)
String WebKitCSSTransformValue::customSerializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    StringBuilder result;
    if (m_type != UnknownTransformOperation) {
        ASSERT(static_cast<size_t>(m_type) < WTF_ARRAY_LENGTH(transformName));
        result.append(transformName[m_type]);
        result.append('(');
        result.append(CSSValueList::customSerializeResolvingVariables(variables));
        result.append(')');
    }
    return result.toString();
}
#endif

WebKitCSSTransformValue::WebKitCSSTransformValue(const WebKitCSSTransformValue& cloneFrom)
    : CSSValueList(cloneFrom)
    , m_type(cloneFrom.m_type)
{
}

PassRefPtr<WebKitCSSTransformValue> WebKitCSSTransformValue::cloneForCSSOM() const
{
    return adoptRef(new WebKitCSSTransformValue(*this));
}

void WebKitCSSTransformValue::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    CSSValueList::reportDescendantMemoryUsage(memoryObjectInfo);
}

}
