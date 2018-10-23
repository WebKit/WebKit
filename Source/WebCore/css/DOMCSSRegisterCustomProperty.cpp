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
#include "StyleBuilderConverter.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

ExceptionOr<void> DOMCSSRegisterCustomProperty::registerProperty(Document& document, const DOMCSSCustomPropertyDescriptor& descriptor)
{
    RefPtr<CSSCustomPropertyValue> initialValue;
    if (!descriptor.initialValue.isEmpty()) {
        CSSTokenizer tokenizer(descriptor.initialValue);
        StyleResolver styleResolver(document);

        // We need to initialize this so that we can successfully parse computationally dependent values (like em units).
        // We don't actually need the values to be accurate, since they will be rejected later anyway
        styleResolver.state().setStyle(styleResolver.defaultStyleForElement());
        styleResolver.updateFont();

        initialValue = CSSPropertyParser::parseTypedCustomPropertyValue(descriptor.name, descriptor.syntax, tokenizer.tokenRange(), styleResolver, strictCSSParserContext());

        if (!initialValue || !initialValue->isResolved())
            return Exception { SyntaxError, "The given initial value does not parse for the given syntax." };

        HashSet<CSSPropertyID> dependencies;
        initialValue->collectDirectComputationalDependencies(dependencies);
        initialValue->collectDirectRootComputationalDependencies(dependencies);

        if (!dependencies.isEmpty())
            return Exception { SyntaxError, "The given initial value must be computationally independent." };
    }

    CSSRegisteredCustomProperty property { descriptor.name, descriptor.syntax, descriptor.inherits, WTFMove(initialValue) };
    if (!document.registerCSSProperty(WTFMove(property)))
        return Exception { InvalidModificationError, "This property has already been registered." };

    document.styleScope().didChangeStyleSheetEnvironment();

    return { };
}

DOMCSSRegisterCustomProperty* DOMCSSRegisterCustomProperty::from(DOMCSSNamespace& css)
{
    auto* supplement = static_cast<DOMCSSRegisterCustomProperty*>(Supplement<DOMCSSNamespace>::from(&css, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<DOMCSSRegisterCustomProperty>(css);
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
