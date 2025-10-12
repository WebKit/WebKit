/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSValue.h>
#include <WebCore/CSSValueAggregates.h>
#include <WebCore/CSSVariableData.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StyleImageWrapper.h>
#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StyleTransformFunction.h>
#include <WebCore/StyleURL.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CSSValuePool;

namespace Style {

class CustomProperty final : public RefCounted<CustomProperty> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CustomProperty);
public:
    // https://drafts.csswg.org/css-variables-2/#guaranteed-invalid
    struct GuaranteedInvalid { };

    using Value = Variant<
        LengthPercentage<>,
        Length<>,
        Number<>,
        Percentage<>,
        Angle<>,
        Time<>,
        Resolution<>,
        ImageWrapper,
        Color,
        URL,
        CustomIdentifier,
        String,
        TransformFunction
    >;

    struct ValueList {
        Vector<Value> values;
        CSSValue::ValueSeparator separator;
        bool operator==(const ValueList&) const = default;
    };

    using Kind = Variant<
        GuaranteedInvalid,
        Ref<CSSVariableData>,
        Value,
        ValueList
    >;

    static Ref<const CustomProperty> createForGuaranteedInvalid(const AtomString& name);
    static Ref<const CustomProperty> createForVariableData(const AtomString& name, Ref<CSSVariableData>&&);
    static Ref<const CustomProperty> createForValue(const AtomString& name, Value&&);
    static Ref<const CustomProperty> createForValueList(const AtomString& name, ValueList&&);

    const AtomString& name() const { return m_name; }
    const Kind& value() const { return m_value; }

    const Vector<CSSParserToken>& tokens() const;

    bool isGuaranteedInvalid() const;
    bool isVariableData() const;
    bool isValue() const;
    bool isValueList() const;

    bool isAnimatable() const;

    bool operator==(const CustomProperty&) const;

    Ref<CSSValue> propertyValue(CSSValuePool&, const RenderStyle&) const;
    String propertyValueSerialization(const CSS::SerializationContext&, const RenderStyle&) const;
    void propertyValueSerialization(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&) const;

private:
    CustomProperty(const AtomString& name, Kind&& value)
        : m_name(name)
        , m_value(WTFMove(value))
    {
    }

    String propertyValueSerializationForTokenization(const CSS::SerializationContext&, const RenderStyle&) const;
    void propertyValueSerializationForTokenization(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&) const;

    const AtomString m_name;
    const Kind m_value;
    mutable RefPtr<CSSVariableData> m_cachedTokens;
};

inline Ref<const CustomProperty> CustomProperty::createForGuaranteedInvalid(const AtomString& name)
{
    return adoptRef(*new CustomProperty(name, Kind { GuaranteedInvalid { } }));
}

inline Ref<const CustomProperty> CustomProperty::createForVariableData(const AtomString& name, Ref<CSSVariableData>&& value)
{
    return adoptRef(*new CustomProperty(name, Kind { WTF::InPlaceType<Ref<CSSVariableData>>, WTFMove(value) }));
}

inline Ref<const CustomProperty> CustomProperty::createForValue(const AtomString& name, Value&& value)
{
    return adoptRef(*new CustomProperty(name, Kind { WTFMove(value) }));
}

inline Ref<const CustomProperty> CustomProperty::createForValueList(const AtomString& name, ValueList&& valueList)
{
    return adoptRef(*new CustomProperty(name, Kind { WTFMove(valueList) }));
}

inline bool CustomProperty::isGuaranteedInvalid() const
{
    return std::holds_alternative<GuaranteedInvalid>(m_value);
}

inline bool CustomProperty::isVariableData() const
{
    return std::holds_alternative<Ref<CSSVariableData>>(m_value);
}

inline bool CustomProperty::isValue() const
{
    return std::holds_alternative<Value>(m_value);
}

inline bool CustomProperty::isValueList() const
{
    return std::holds_alternative<ValueList>(m_value);
}

inline bool CustomProperty::isAnimatable() const
{
    return std::holds_alternative<Value>(m_value)
        || std::holds_alternative<ValueList>(m_value);
}

} // namespace Style
} // namespace WebCore
