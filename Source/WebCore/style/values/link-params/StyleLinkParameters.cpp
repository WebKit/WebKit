/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StyleLinkParameters.h"

#include "CSSDeclarationValue.h"
#include "CSSLinkParameter.h"
#include "CSSParamValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParser.h"
#include "CSSTokenizer.h"
#include "CSSURLModifiers.h"
#include "CSSValueKeywords.h"
#include "CSSVariableData.h"
#include "ColorSerialization.h"
#include "StyleBuilderChecking.h"
#include "StyleBuilderState.h"
#include "StyleColor.h"
#include "StyleColorResolver.h"
#include "StyleCustomProperty.h"

namespace WebCore {
namespace Style {

static ParamSpec toStyleParamSpec(const CSS::ParamSpec& spec)
{
    return WTF::switchOn(spec.value,
        [](CSS::Keyword::Color keyword) { return ParamSpec { keyword }; },
        [](CSS::Keyword::AccentColor keyword) { return ParamSpec { keyword }; },
        [](const CSS::ParamSpec::Custom& custom) {
            return ParamSpec { ParamSpec::Custom { CustomIdent { custom.name.value }, custom.type } };
        }
    );
}

static CSS::ParamSpec toCSSParamSpec(const ParamSpec& spec)
{
    return WTF::switchOn(spec.value,
        [](CSS::Keyword::Color keyword) { return CSS::ParamSpec { keyword }; },
        [](CSS::Keyword::AccentColor keyword) { return CSS::ParamSpec { keyword }; },
        [](const ParamSpec::Custom& custom) {
            return CSS::ParamSpec { CSS::ParamSpec::Custom { CSS::CustomIdent { custom.name.value }, custom.type } };
        }
    );
}

const AtomString& ParamSpec::name() const
{
    return WTF::switchOn(value,
        [](CSS::Keyword::Color) -> const AtomString& { return nameStringForSerialization(CSSValueColor); },
        [](CSS::Keyword::AccentColor) -> const AtomString& { return nameStringForSerialization(CSSValueAccentColor); },
        [](const Custom& custom) -> const AtomString& { return custom.name.value; }
    );
}

LinkParameters linkParametersForResource(const LinkParameters& fromProperty, const CSS::URLModifiers& fromURL)
{
    auto& fromURLParameters = fromURL.linkParameters;
    if (fromURLParameters.isEmpty())
        return fromProperty;

    auto countFromProperty = fromProperty.size();

    return LinkParameterList::createWithSizeFromGenerator(countFromProperty + fromURLParameters.size(), [&](size_t i) -> ParamFunction {
        if (i < countFromProperty)
            return fromProperty[i];

        auto& parameter = fromURLParameters[i - countFromProperty];
        return ParamFunction { LinkParameter { toStyleParamSpec(parameter->spec), DeclarationValue { parameter->value.value.copyRef() } } };
    });
}

static LinkParameter::ResolvedValue resolveTypedValue(const ParamSpec& spec, const DeclarationValue& value, BuilderState& state)
{
    // Untyped parameters pass through unchanged.
    auto* custom = std::get_if<ParamSpec::Custom>(&spec.value);
    if (!custom || !custom->type)
        return LinkParameter::Unresolved { };

    auto parsed = CSSPropertyParser::parseTypedCustomPropertyValue(spec.name(), custom->type->syntax, value.value->tokenRange(), state, value.value->context(), value.value->isAttrTainted());
    if (!parsed)
        return LinkParameter::Invalid { };

    RefPtr<const CustomProperty> property;
    WTF::switchOn(*parsed,
        [&](const Ref<const CustomProperty>& resolvedProperty) { property = resolvedProperty.ptr(); },
        [](CSSWideKeyword) { }
    );
    if (!property)
        return LinkParameter::Invalid { };

    // A computed <color> can still reference the element, through currentcolor or a function
    // containing it, so it is absolutized here.
    WTF::String serialization;
    if (auto* typedValue = std::get_if<CustomProperty::Value>(&property->value())) {
        if (auto* color = std::get_if<Color>(typedValue))
            serialization = WebCore::serializationForCSS(ColorResolver { state.style() }.colorResolvingCurrentColor(*color));
    }

    if (serialization.isNull())
        return Ref { CSSVariableData::create(CSSParserTokenRange { property->tokens().span() }) };

    CSSTokenizer tokenizer { StringView { serialization } };
    return Ref { CSSVariableData::create(tokenizer.tokenRange()) };
}

auto CSSValueConversion<LinkParameter>::operator()(BuilderState& state, const CSSValue& value) -> LinkParameter
{
    RefPtr parameter = requiredDowncast<CSSParamValue>(state, value);
    if (!parameter)
        return { ParamSpec { ParamSpec::Custom { CustomIdent { nullAtom() }, std::nullopt } }, DeclarationValue { CSSVariableData::create(CSSParserTokenRange { }) }, LinkParameter::Unresolved { } };

    auto spec = toStyleParamSpec(parameter->parameter()->spec);
    auto declarationValue = toStyle(parameter->parameter()->value, state);
    auto resolved = resolveTypedValue(spec, declarationValue, state);

    return { WTF::move(spec), WTF::move(declarationValue), WTF::move(resolved) };
}

auto CSSValueCreation<LinkParameter>::operator()(CSSValuePool&, const ComputedStyle& style, const LinkParameter& parameter) -> Ref<CSSValue>
{
    return CSSParamValue::create(CSS::ParamFunction { CSS::LinkParameter { toCSSParamSpec(parameter.spec), toCSS(parameter.value, style) } });
}

} // namespace Style
} // namespace WebCore
