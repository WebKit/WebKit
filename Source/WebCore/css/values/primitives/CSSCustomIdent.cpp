/*
 * Copyright (C) 2026 saku
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

#include "config.h"
#include "CSSCustomIdent.h"

#include "CSSCustomIdentValue.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSSerializationContext.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include <wtf/Hasher.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSS {

bool CustomIdent::isNull() const
{
    auto* resolved = std::get_if<AtomString>(&value);
    return resolved && resolved->isNull();
}

bool CustomIdent::startsWith(StringView prefix) const
{
    return WTF::switchOn(value,
        [&](const AtomString& resolved) {
            return resolved.startsWith(prefix);
        },
        [&](const IdentFunction& function) {
            // Build the identifier with every <integer> argument treated as "0", then test the prefix.
            // https://github.com/w3c/csswg-drafts/issues/12206#issuecomment-3998743769
            StringBuilder builder;
            for (auto& argument : function.parameters) {
                WTF::switchOn(argument,
                    [&](const IdentFunctionIdent& ident) { builder.append(ident.value); },
                    [&](const String& string) { builder.append(string.value); },
                    [&](const Integer<>&) { builder.append('0'); }
                );
            }
            return StringView { builder }.startsWith(prefix);
        }
    );
}

// MARK: - Equality

SUPPRESS_NODELETE bool CustomIdent::operator==(const CustomIdent& other) const
{
    return value == other.value;
}

void Serialize<IdentFunctionIdent>::operator()(StringBuilder& builder, const SerializationContext&, const IdentFunctionIdent& value)
{
    WebCore::serializeIdentifier(builder, value.value);
}

void Serialize<CustomIdent>::operator()(StringBuilder& builder, const SerializationContext& context, const CustomIdent& value)
{
    WTF::switchOn(value.value,
        [&](const AtomString& resolved) {
            WebCore::serializeIdentifier(builder, resolved);
        },
        [&](const IdentFunction& function) {
            serializationForCSS(builder, context, function);
        }
    );
}

void ComputedStyleDependenciesCollector<CustomIdent>::operator()(ComputedStyleDependencies& dependencies, const CustomIdent& value)
{
    WTF::switchOn(value.value,
        [&](const AtomString&) { },
        [&](const IdentFunction& function) {
            collectComputedStyleDependencies(dependencies, function);
        }
    );
}

IterationStatus CSSValueChildrenVisitor<CustomIdent>::operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CustomIdent& value)
{
    return WTF::switchOn(value.value,
        [&](const AtomString&) {
            return IterationStatus::Continue;
        },
        [&](const IdentFunction& function) {
            return visitCSSValueChildren(func, function);
        }
    );
}

Ref<CSSValue> CSSValueCreation<CustomIdent>::operator()(CSSValuePool&, const CustomIdent& value)
{
    return CSSCustomIdentValue::create(value);
}

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<CustomIdent>::operator()(CSSValuePool&, CSSStyleDeclaration& owner, const CustomIdent& value)
{
    return DeprecatedCSSOMPrimitiveValue::create(value, owner);
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const CustomIdent& value)
{
    WTF::switchOn(value.value,
        [&](const AtomString& resolved) {
            ts << resolved;
        },
        [&](const IdentFunction&) {
            ts << serializationForCSS(defaultSerializationContext(), value);
        }
    );
    return ts;
}

// MARK: - Hashing

SUPPRESS_NODELETE void add(Hasher& hasher, const CustomIdent& value)
{
    add(hasher, value.value.index());

    WTF::switchOn(value.value,
        [&](const AtomString& resolved) {
            add(hasher, resolved);
        },
        [&](const IdentFunction& function) {
            // Hash the serialization; an <integer> argument may be an unevaluated calc with no raw value.
            add(hasher, serializationForCSS(defaultSerializationContext(), function));
        }
    );
}

} // namespace CSS
} // namespace WebCore
