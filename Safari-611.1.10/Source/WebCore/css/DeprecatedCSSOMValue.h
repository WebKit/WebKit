/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSStyleDeclaration.h"
#include "CSSValue.h"
#include "ExceptionOr.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DeprecatedCSSOMValue : public RefCounted<DeprecatedCSSOMValue>, public CanMakeWeakPtr<DeprecatedCSSOMValue> {
public:
    // Exactly match the IDL. No reason to add anything if it's not in the IDL.
    enum Type {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3
    };

    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref()
    {
        if (derefBase())
            destroy();
    }

    WEBCORE_EXPORT unsigned cssValueType() const;

    WEBCORE_EXPORT String cssText() const;
    ExceptionOr<void> setCssText(const String&) { return { }; } // Will never implement.

    bool isComplexValue() const { return m_classType == DeprecatedComplexValueClass; }
    bool isPrimitiveValue() const { return m_classType == DeprecatedPrimitiveValueClass; }
    bool isValueList() const { return m_classType == DeprecatedValueListClass; }

    CSSStyleDeclaration& owner() const { return m_owner; }

protected:
    static const size_t ClassTypeBits = 2;
    enum DeprecatedClassType {
        DeprecatedComplexValueClass,
        DeprecatedPrimitiveValueClass,
        DeprecatedValueListClass
    };

    DeprecatedClassType classType() const { return static_cast<DeprecatedClassType>(m_classType); }

    DeprecatedCSSOMValue(DeprecatedClassType classType, CSSStyleDeclaration& owner)
        : m_classType(classType)
        , m_owner(owner)
    {
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!
    ~DeprecatedCSSOMValue() = default;

private:
    WEBCORE_EXPORT void destroy();

protected:
    unsigned m_valueListSeparator : CSSValue::ValueListSeparatorBits;
    unsigned m_classType : ClassTypeBits; // ClassType
    
    Ref<CSSStyleDeclaration> m_owner;
};

class DeprecatedCSSOMComplexValue : public DeprecatedCSSOMValue {
public:
    static Ref<DeprecatedCSSOMComplexValue> create(const CSSValue& value, CSSStyleDeclaration& owner)
    {
        return adoptRef(*new DeprecatedCSSOMComplexValue(value, owner));
    }

    String cssText() const { return m_value->cssText(); }

    unsigned cssValueType() const { return m_value->cssValueType(); }

protected:
    DeprecatedCSSOMComplexValue(const CSSValue& value, CSSStyleDeclaration& owner)
        : DeprecatedCSSOMValue(DeprecatedComplexValueClass, owner)
        , m_value(const_cast<CSSValue&>(value))
    {
    }

private:
    Ref<CSSValue> m_value;
};
    
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSSOM_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::DeprecatedCSSOMValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CSSOM_VALUE(DeprecatedCSSOMComplexValue, isComplexValue())


