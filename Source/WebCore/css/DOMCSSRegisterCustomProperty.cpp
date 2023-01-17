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
#include "CustomPropertyRegistry.h"
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

    if (!syntax->isUniversal() && descriptor.initialValue.isNull())
        return Exception { SyntaxError, "An initial value is mandatory except for the '*' syntax."_s };

    RefPtr<CSSCustomPropertyValue> initialValue;
    if (!descriptor.initialValue.isNull()) {
        CSSTokenizer tokenizer(descriptor.initialValue);

        auto dependencies = CSSPropertyParser::collectParsedCustomPropertyValueDependencies(*syntax, tokenizer.tokenRange(), strictCSSParserContext());

        if (!dependencies.isEmpty())
            return Exception { SyntaxError, "The given initial value must be computationally independent."_s };

        // We don't need to provide a real context style since only computationally independent values are allowed (no 'em' etc).
        auto placeholderStyle = RenderStyle::create();
        Style::Builder builder { placeholderStyle, { document, RenderStyle::defaultStyle() }, { }, { } };

        initialValue = CSSPropertyParser::parseTypedCustomPropertyInitialValue(descriptor.name, *syntax, tokenizer.tokenRange(), builder.state(), { document });

        if (!initialValue)
            return Exception { SyntaxError, "The given initial value does not parse for the given syntax."_s };
    }

    auto property = CSSRegisteredCustomProperty {
        descriptor.name,
        *syntax,
        descriptor.inherits,
        WTFMove(initialValue)
    };

    auto& registry = document.styleScope().customPropertyRegistry();
    if (!registry.registerFromAPI(WTFMove(property)))
        return Exception { InvalidModificationError, "This property has already been registered."_s };

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
