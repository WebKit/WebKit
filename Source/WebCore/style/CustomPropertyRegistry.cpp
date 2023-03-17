/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CustomPropertyRegistry.h"

#include "CSSCustomPropertyValue.h"
#include "CSSPropertyParser.h"
#include "CSSRegisteredCustomProperty.h"
#include "Document.h"
#include "Element.h"
#include "KeyframeEffect.h"
#include "RenderStyle.h"
#include "StyleBuilder.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "WebAnimation.h"

namespace WebCore {
namespace Style {

CustomPropertyRegistry::CustomPropertyRegistry(Scope& scope)
    : m_scope(scope)
    , m_initialValuePrototypeStyle(RenderStyle::createPtr())
{
}

const CSSRegisteredCustomProperty* CustomPropertyRegistry::get(const AtomString& name) const
{
    ASSERT(isCustomPropertyName(name));

    // API wins.
    // https://drafts.css-houdini.org/css-properties-values-api/#determining-registration
    if (auto* property = m_propertiesFromAPI.get(name))
        return property;

    return m_propertiesFromStylesheet.get(name);
}

bool CustomPropertyRegistry::isInherited(const AtomString& name) const
{
    auto* registered = get(name);
    return registered ? registered->inherits : true;
}


bool CustomPropertyRegistry::registerFromAPI(CSSRegisteredCustomProperty&& property)
{
    // First registration wins.
    // https://drafts.css-houdini.org/css-properties-values-api/#determining-registration
    auto success = m_propertiesFromAPI.ensure(property.name, [&] {
        return makeUnique<const CSSRegisteredCustomProperty>(WTFMove(property));
    }).isNewEntry;

    if (success) {
        invalidate(property.name);
        m_scope.didChangeStyleSheetEnvironment();
    }

    return success;
}

void CustomPropertyRegistry::registerFromStylesheet(const StyleRuleProperty::Descriptor& descriptor)
{
    // "The inherits descriptor is required for the @property rule to be valid; if it’s missing, the @property rule is invalid."
    // https://drafts.css-houdini.org/css-properties-values-api/#inherits-descriptor
    if (!descriptor.inherits)
        return;

    // "If the provided string is not a valid syntax string, the descriptor is invalid and must be ignored."
    // https://drafts.css-houdini.org/css-properties-values-api/#the-syntax-descriptor
    auto syntax = CSSCustomPropertySyntax::parse(descriptor.syntax);
    if (!syntax)
        return;

    auto& document = m_scope.document();

    auto initialValue = [&]() -> RefPtr<CSSCustomPropertyValue> {
        if (!descriptor.initialValue) {
            // "If the value of the syntax descriptor is the universal syntax definition, then the initial-value descriptor is optional.
            //  If omitted, the initial value of the property is the guaranteed-invalid value."
            // https://drafts.css-houdini.org/css-properties-values-api/#initial-value-descriptor
            if (syntax->isUniversal())
                return CSSCustomPropertyValue::createWithID(descriptor.name, CSSValueInvalid);

            return nullptr;
        }

        auto tokenRange = descriptor.initialValue->tokenRange();

        // FIXME: This parses twice.
        auto dependencies = CSSPropertyParser::collectParsedCustomPropertyValueDependencies(*syntax, tokenRange, strictCSSParserContext());
        if (!dependencies.isEmpty())
            return nullptr;

        // We don't need to provide a real context style since only computationally independent values are allowed (no 'em' etc).
        auto placeholderStyle = RenderStyle::create();
        Style::Builder builder { placeholderStyle, { document, RenderStyle::defaultStyle() }, { }, { } };

        return CSSPropertyParser::parseTypedCustomPropertyInitialValue(descriptor.name, *syntax, tokenRange, builder.state(), { document });
    }();

    if (!initialValue)
        return;

    auto property = CSSRegisteredCustomProperty {
        AtomString { descriptor.name },
        *syntax,
        *descriptor.inherits,
        WTFMove(initialValue)
    };

    // Last rule wins.
    // https://drafts.css-houdini.org/css-properties-values-api/#determining-registration
    m_propertiesFromStylesheet.set(property.name, makeUnique<const CSSRegisteredCustomProperty>(WTFMove(property)));

    invalidate(property.name);
}

void CustomPropertyRegistry::clearRegisteredFromStylesheets()
{
    if (m_propertiesFromStylesheet.isEmpty())
        return;

    m_propertiesFromStylesheet.clear();
    invalidate(nullAtom());
}

const RenderStyle& CustomPropertyRegistry::initialValuePrototypeStyle() const
{
    if (m_hasInvalidPrototypeStyle) {
        m_hasInvalidPrototypeStyle = false;

        auto oldStyle = std::exchange(m_initialValuePrototypeStyle, RenderStyle::createPtr());

        auto initializeToStyle = [&](auto& map) {
            for (auto& property : map.values()) {
                if (property->initialValue && !property->initialValue->isInvalid())
                    m_initialValuePrototypeStyle->setCustomPropertyValue(*property->initialValue, property->inherits);
            }
        };

        initializeToStyle(m_propertiesFromStylesheet);
        initializeToStyle(m_propertiesFromAPI);

        m_initialValuePrototypeStyle->deduplicateCustomProperties(*oldStyle);
    }

    return *m_initialValuePrototypeStyle;
}

void CustomPropertyRegistry::invalidate(const AtomString& customProperty)
{
    m_hasInvalidPrototypeStyle = true;
    // Changing property registration may affect computed property values in the cache.
    m_scope.invalidateMatchedDeclarationsCache();

    // FIXME: Invalidate all?
    if (!customProperty.isNull())
        notifyAnimationsOfCustomPropertyRegistration(customProperty);
}

void CustomPropertyRegistry::notifyAnimationsOfCustomPropertyRegistration(const AtomString& customProperty)
{
    auto& document = m_scope.document();
    for (auto* animation : WebAnimation::instances()) {
        if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(animation->effect())) {
            if (auto* target = keyframeEffect->target()) {
                if (&target->document() == &document)
                    keyframeEffect->customPropertyRegistrationDidChange(customProperty);
            }
        }
    }
}

}
}
