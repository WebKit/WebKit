/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "DOMCSSRegisterCustomProperty.h"

#include "CSSCustomPropertyValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSTokenizer.h"
#include "DOMCSSNamespace.h"
#include "Document.h"
#include "StyleBuilder.h"
#include "StyleBuilderConverter.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

ExceptionOr<void> DOMCSSRegisterCustomProperty::registerProperty(Document& document, const DOMCSSCustomPropertyDescriptor& descriptor)
{
    if (!isCustomPropertyName(descriptor.name))
        return Exception { SyntaxError, "The name of this property is not a custom property name."_s };

    auto syntax = CSSCustomPropertySyntax::parse(descriptor.syntax);
    if (!syntax)
        return Exception { SyntaxError, "Invalid property syntax definition."_s };

    RefPtr<CSSCustomPropertyValue> initialValue;
    if (!descriptor.initialValue.isNull()) {
        CSSTokenizer tokenizer(descriptor.initialValue);
        auto styleResolver = Style::Resolver::create(document);

        // We need to initialize this so that we can successfully parse computationally dependent values (like em units).
        // We don't actually need the values to be accurate, since they will be rejected later anyway
        auto style = styleResolver->defaultStyleForElement(nullptr);

        HashSet<CSSPropertyID> dependencies;
        CSSPropertyParser::collectParsedCustomPropertyValueDependencies(*syntax, true /* isInitial */, dependencies, tokenizer.tokenRange(), strictCSSParserContext());

        if (!dependencies.isEmpty())
            return Exception { SyntaxError, "The given initial value must be computationally independent."_s };

        Style::MatchResult matchResult;

        auto parentStyle = RenderStyle::clone(*style);
        Style::Builder dummyBuilder(*style, { document, parentStyle }, matchResult, { });

        initialValue = CSSPropertyParser::parseTypedCustomPropertyValue(descriptor.name, *syntax, tokenizer.tokenRange(), dummyBuilder.state(), { document });

        if (!initialValue || !initialValue->isResolved())
            return Exception { SyntaxError, "The given initial value does not parse for the given syntax."_s };

        if (initialValue->containsCSSWideKeyword())
            return Exception { SyntaxError, "The intitial value cannot be a CSS-wide keyword."_s };
    }

    CSSRegisteredCustomProperty property { AtomString { descriptor.name }, *syntax, descriptor.inherits, WTFMove(initialValue) };
    if (!document.registerCSSCustomProperty(WTFMove(property)))
        return Exception { InvalidModificationError, "This property has already been registered."_s };

    document.styleScope().didChangeStyleSheetEnvironment();

    return { };
}

DOMCSSRegisterCustomProperty* DOMCSSRegisterCustomProperty::from(DOMCSSNamespace& css)
{
    auto* supplement = static_cast<DOMCSSRegisterCustomProperty*>(Supplement<DOMCSSNamespace>::from(&css, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DOMCSSRegisterCustomProperty>(css);
        supplement = newSupplement.get();
        provideTo(&css, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

const char* DOMCSSRegisterCustomProperty::supplementName()
{
    return "DOMCSSRegisterCustomProperty";
}

}
