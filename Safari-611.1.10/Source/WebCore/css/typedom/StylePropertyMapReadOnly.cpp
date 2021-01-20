/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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
#include "StylePropertyMapReadOnly.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSCustomPropertyValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "Document.h"
#include "TypedOMCSSImageValue.h"
#include "TypedOMCSSStyleValue.h"
#include "TypedOMCSSUnitValue.h"
#include "TypedOMCSSUnparsedValue.h"
#include <wtf/HashMap.h>

namespace WebCore {


RefPtr<TypedOMCSSStyleValue> StylePropertyMapReadOnly::reifyValue(CSSValue* value, Document& document, Element*)
{
    if (!value)
        return nullptr;

    // FIXME: Properly reify all length values.
    if (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).primitiveType() == CSSUnitType::CSS_PX)
        return TypedOMCSSUnitValue::create(downcast<CSSPrimitiveValue>(*value).doubleValue(), "px");

    if (is<CSSImageValue>(*value))
        return TypedOMCSSImageValue::create(downcast<CSSImageValue>(*value), document);

    // FIXME: should use raw TypedOMCSSStyleValue
    return TypedOMCSSUnparsedValue::create(value->cssText());
}

RefPtr<TypedOMCSSStyleValue> StylePropertyMapReadOnly::customPropertyValueOrDefault(const String& name, Document& document, CSSValue* inputValue, Element* element)
{
    if (!inputValue) {
        auto* registered = document.getCSSRegisteredCustomPropertySet().get(name);

        if (registered && registered->initialValue()) {
            auto value = registered->initialValueCopy();
            return StylePropertyMapReadOnly::reifyValue(value.get(), document, element);
        }

        return TypedOMCSSUnparsedValue::create(emptyString());
    }

    return StylePropertyMapReadOnly::reifyValue(inputValue, document, element);
}

} // namespace WebCore

#endif
