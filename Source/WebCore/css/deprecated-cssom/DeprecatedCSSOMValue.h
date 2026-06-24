/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSStyleDeclaration.h>
#include <WebCore/ExceptionOr.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DeprecatedCSSOMValue : public RefCountedAndCanMakeWeakPtr<DeprecatedCSSOMValue> {
public:
    // Exactly match the IDL. No reason to add anything if it's not in the IDL.
    enum Type : unsigned short {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3
    };

    WEBCORE_EXPORT virtual ~DeprecatedCSSOMValue();

    WEBCORE_EXPORT virtual unsigned short NODELETE cssValueType() const = 0;
    WEBCORE_EXPORT virtual String cssText() const = 0;
    ExceptionOr<void> setCssText(const String&) { return { }; } // Will never implement.

    virtual bool isCustomValue() const { return false; }
    virtual bool isPrimitiveValue() const { return false; }
    virtual bool isValueList() const { return false; }

    CSSStyleDeclaration& owner() const { return m_owner; }

protected:
    DeprecatedCSSOMValue(CSSStyleDeclaration& owner)
        : m_owner(owner)
    {
    }

    const Ref<CSSStyleDeclaration> m_owner;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSSOM_VALUE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::DeprecatedCSSOMValue& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

