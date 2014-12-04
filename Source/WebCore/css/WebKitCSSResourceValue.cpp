/*
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitCSSResourceValue.h"

#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WebKitCSSResourceValue::WebKitCSSResourceValue(PassRefPtr<CSSValue> resourceValue)
    : CSSValue(WebKitCSSResourceClass)
    , m_innerValue(resourceValue)
{
}

String WebKitCSSResourceValue::customCSSText() const
{
    if (isCSSValueNone())
        return "none";

    if (m_innerValue.get()) {
        if (is<CSSPrimitiveValue>(m_innerValue.get()) && downcast<CSSPrimitiveValue>(m_innerValue.get())->isURI()) {
            StringBuilder result;
            result.appendLiteral("url(");
            result.append(quoteCSSURLIfNeeded(downcast<CSSPrimitiveValue>(m_innerValue.get())->getStringValue()));
            result.appendLiteral(")");
            return result.toString();
        }
        
        return m_innerValue->cssText();
    }
    
    return "";
}

bool WebKitCSSResourceValue::isCSSValueNone() const
{
    if (is<CSSPrimitiveValue>(m_innerValue.get())) {
        RefPtr<CSSPrimitiveValue> primitiveValue = downcast<CSSPrimitiveValue>(m_innerValue.get());
        return (primitiveValue->isValueID() && primitiveValue->getValueID() == CSSValueNone);
    }

    return false;
}

}
