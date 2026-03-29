/*
 * Copyright (C) 2026 saku (saku@email.sakupi01.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"
#include "CSSValue.h"
#include <WebCore/CSSValueAggregates.h>
#include <optional>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSToLengthConversionData;

// Represents a single argument in the ident() function.
// https://drafts.csswg.org/css-values-5/#ident
// <ident()> = ident( <ident-arg>+ )
// <ident-arg> = <string> | <integer> | <ident>
struct IdentArgument {
    using Value = Variant<String, CSS::Integer<>, CSS::CustomIdent>;

    Value value;

    explicit IdentArgument(String&& stringArgument)
        : value(WTF::move(stringArgument))
    {
    }

    explicit IdentArgument(CSS::Integer<>&& integerArgument)
        : value(WTF::move(integerArgument))
    {
    }

    explicit IdentArgument(CSS::CustomIdent&& identArgument)
        : value(WTF::move(identArgument))
    {
    }

    bool operator==(const IdentArgument& other) const;
};

// Represents the CSS ident() function value.
// https://drafts.csswg.org/css-values-5/#ident
class CSSIdentValue final : public CSSValue {
public:
    static Ref<CSSIdentValue> create(Vector<IdentArgument>&& arguments);

    const Vector<IdentArgument>& arguments() const { return m_arguments; }

    String stringValue() const;
    std::optional<String> computedStringValue(const CSSToLengthConversionData&) const;

    bool equals(const CSSIdentValue& other) const;
    String customCSSText(const CSS::SerializationContext&) const;

private:
    explicit CSSIdentValue(Vector<IdentArgument>&& arguments)
        : CSSValue(ClassType::Ident)
        , m_arguments(WTF::move(arguments))
    {
    }

    Vector<IdentArgument> m_arguments;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSIdentValue, isIdentValue())
