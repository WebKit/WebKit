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

#include "CSSPropertyParser.h"
#include "CSSRegisteredCustomProperty.h"
#include "StyleBuilder.h"
#include "StyleResolver.h"
#include "StyleScope.h"

namespace WebCore {
namespace Style {

CustomPropertyRegistry::CustomPropertyRegistry(Scope& scope)
    : m_scope(scope)
{
}

const CSSRegisteredCustomProperty* CustomPropertyRegistry::get(const AtomString& name) const
{
    // API wins.
    // https://drafts.css-houdini.org/css-properties-values-api/#determining-registration
    if (auto* property = m_propertiesFromAPI.get(name))
        return property;

    return m_propertiesFromStylesheet.get(name);
}

bool CustomPropertyRegistry::registerFromAPI(CSSRegisteredCustomProperty&& property)
{
    // First registration wins.
    // https://drafts.css-houdini.org/css-properties-values-api/#determining-registration
    auto success = m_propertiesFromAPI.ensure(property.name, [&] {
        return makeUnique<const CSSRegisteredCustomProperty>(WTFMove(property));
    }).isNewEntry;

    if (success)
        m_scope.didChangeStyleSheetEnvironment();

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
        HashSet<CSSPropertyID> dependencies;
        CSSPropertyParser::collectParsedCustomPropertyValueDependencies(*syntax, true /* isInitial */, dependencies, tokenRange, strictCSSParserContext());
        if (!dependencies.isEmpty())
            return nullptr;

        auto style = RenderStyle::clone(*m_scope.resolver().rootDefaultStyle());
        Style::Builder dummyBuilder(style, { document, style }, { }, { });

        return CSSPropertyParser::parseTypedCustomPropertyInitialValue(descriptor.name, *syntax, tokenRange, dummyBuilder.state(), { document });
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
}

void CustomPropertyRegistry::clearRegisteredFromStylesheets()
{
    m_propertiesFromStylesheet.clear();
}

}
}
