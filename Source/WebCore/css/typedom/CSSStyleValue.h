/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CSSValue.h>
#include <WebCore/ScriptWrappable.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<typename T> class ExceptionOr;
struct CSSParserContext;
class Document;

enum class CSSStyleValueType : uint8_t {
    CSSStyleValue,
    CSSStyleImageValue,
    CSSTransformValue,
    CSSMathClamp,
    CSSMathInvert,
    CSSMathMin,
    CSSMathMax,
    CSSMathNegate,
    CSSMathProduct,
    CSSMathSum,
    CSSColorCSSOMColor,
    CSSColorHSL,
    CSSColorHWB,
    CSSColorLab,
    CSSColorLCH,
    CSSColorOKLCH,
    CSSColorOKLab,
    CSSColorRGB,
    CSSUnitValue,
    CSSUnparsedValue,
    CSSOMKeywordValue
};

inline bool isCSSNumericValue(CSSStyleValueType type)
{
    switch (type) {
    case CSSStyleValueType::CSSMathClamp:
    case CSSStyleValueType::CSSMathInvert:
    case CSSStyleValueType::CSSMathMin:
    case CSSStyleValueType::CSSMathMax:
    case CSSStyleValueType::CSSMathNegate:
    case CSSStyleValueType::CSSMathProduct:
    case CSSStyleValueType::CSSMathSum:
    case CSSStyleValueType::CSSUnitValue:
        return true;
    case CSSStyleValueType::CSSStyleValue:
    case CSSStyleValueType::CSSStyleImageValue:
    case CSSStyleValueType::CSSTransformValue:
    case CSSStyleValueType::CSSUnparsedValue:
    case CSSStyleValueType::CSSOMKeywordValue:
    case CSSStyleValueType::CSSColorCSSOMColor:
    case CSSStyleValueType::CSSColorHSL:
    case CSSStyleValueType::CSSColorHWB:
    case CSSStyleValueType::CSSColorLab:
    case CSSStyleValueType::CSSColorLCH:
    case CSSStyleValueType::CSSColorOKLab:
    case CSSStyleValueType::CSSColorOKLCH:
    case CSSStyleValueType::CSSColorRGB:
        break;
    }
    return false;
}

inline bool isCSSMathValue(CSSStyleValueType type)
{
    switch (type) {
    case CSSStyleValueType::CSSMathClamp:
    case CSSStyleValueType::CSSMathInvert:
    case CSSStyleValueType::CSSMathMin:
    case CSSStyleValueType::CSSMathMax:
    case CSSStyleValueType::CSSMathNegate:
    case CSSStyleValueType::CSSMathProduct:
    case CSSStyleValueType::CSSMathSum:
        return true;
    case CSSStyleValueType::CSSUnitValue:
    case CSSStyleValueType::CSSStyleValue:
    case CSSStyleValueType::CSSStyleImageValue:
    case CSSStyleValueType::CSSTransformValue:
    case CSSStyleValueType::CSSUnparsedValue:
    case CSSStyleValueType::CSSOMKeywordValue:
    case CSSStyleValueType::CSSColorCSSOMColor:
    case CSSStyleValueType::CSSColorHSL:
    case CSSStyleValueType::CSSColorHWB:
    case CSSStyleValueType::CSSColorLab:
    case CSSStyleValueType::CSSColorLCH:
    case CSSStyleValueType::CSSColorOKLab:
    case CSSStyleValueType::CSSColorOKLCH:
    case CSSStyleValueType::CSSColorRGB:
        break;
    }
    return false;
}

inline bool isCSSColorValue(CSSStyleValueType type)
{
    switch (type) {
    case CSSStyleValueType::CSSColorCSSOMColor:
    case CSSStyleValueType::CSSColorHSL:
    case CSSStyleValueType::CSSColorHWB:
    case CSSStyleValueType::CSSColorLab:
    case CSSStyleValueType::CSSColorLCH:
    case CSSStyleValueType::CSSColorOKLab:
    case CSSStyleValueType::CSSColorOKLCH:
    case CSSStyleValueType::CSSColorRGB:
        return true;
    case CSSStyleValueType::CSSMathClamp:
    case CSSStyleValueType::CSSMathInvert:
    case CSSStyleValueType::CSSMathMin:
    case CSSStyleValueType::CSSMathMax:
    case CSSStyleValueType::CSSMathNegate:
    case CSSStyleValueType::CSSMathProduct:
    case CSSStyleValueType::CSSMathSum:
    case CSSStyleValueType::CSSUnitValue:
    case CSSStyleValueType::CSSStyleValue:
    case CSSStyleValueType::CSSStyleImageValue:
    case CSSStyleValueType::CSSTransformValue:
    case CSSStyleValueType::CSSUnparsedValue:
    case CSSStyleValueType::CSSOMKeywordValue:
        break;
    }
    return false;
}

enum class SerializationArguments : uint8_t {
    Nested = 0x1,
    WithoutParentheses = 0x2,
};

struct AssociatedProperty {
    Variant<AtomString, CSSPropertyID> property;

    AssociatedProperty(AtomString&& customPropertyName) : property { WTF::move(customPropertyName) } { }
    AssociatedProperty(const AtomString& customPropertyName) : property { customPropertyName } { }
    AssociatedProperty(CSSPropertyID propertyID) : property { propertyID } { ASSERT(propertyID != CSSPropertyCustom); }

    const AtomString& nameString() const LIFETIME_BOUND
    {
        return WTF::switchOn(property,
            [](const AtomString& customPropertyName) -> const AtomString& { return customPropertyName; },
            [](CSSPropertyID propertyID) -> const AtomString& { return WebCore::nameString(propertyID); }
        );
    }

    std::optional<CSSPropertyID> propertyID() const
    {
        if (auto* propertyID = std::get_if<CSSPropertyID>(&property))
            return std::optional { *propertyID };
        return std::nullopt;
    }

    bool operator==(const AssociatedProperty&) const = default;

    bool operator==(const AtomString& otherCustomPropertyName) const
    {
        if (auto* customPropertyName = std::get_if<AtomString>(&property))
            return *customPropertyName == otherCustomPropertyName;
        return false;
    }

    bool operator==(CSSPropertyID otherPropertyID) const
    {
        if (auto* propertyID = std::get_if<CSSPropertyID>(&property))
            return *propertyID == otherPropertyID;
        return false;
    }
};

class CSSStyleValue : public RefCounted<CSSStyleValue>, public ScriptWrappable {
    WTF_MAKE_TZONE_ALLOCATED(CSSStyleValue);
public:
    String toString() const;
    virtual void serialize(StringBuilder&, OptionSet<SerializationArguments> = { }) const;

IGNORE_GCC_WARNINGS_BEGIN("mismatched-new-delete")
    // https://webkit.org/b/241516
    virtual ~CSSStyleValue();
IGNORE_GCC_WARNINGS_END

    virtual CSSStyleValueType styleValueType() const { return CSSStyleValueType::CSSStyleValue; }

    static ExceptionOr<Ref<CSSStyleValue>> parse(Document&, const AtomString&, const String&);
    static ExceptionOr<Vector<Ref<CSSStyleValue>>> parseAll(Document&, const AtomString&, const String&);

    static Ref<CSSStyleValue> create(RefPtr<CSSValue>&&, AssociatedProperty&&);

    virtual RefPtr<CSSValue> toCSSValue() const { return m_propertyValue; }
    virtual RefPtr<CSSValue> toCSSValueWithProperty(CSSPropertyID) const;

    const std::optional<AssociatedProperty>& associatedProperty() const { return m_associatedProperty; }

protected:
    CSSStyleValue(RefPtr<CSSValue>&&, std::optional<AssociatedProperty>&& = std::nullopt);
    CSSStyleValue() = default;

    std::optional<AssociatedProperty> m_associatedProperty;
    RefPtr<CSSValue> m_propertyValue;
};

} // namespace WebCore
