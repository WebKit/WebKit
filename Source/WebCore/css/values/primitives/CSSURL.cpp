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

#include "config.h"
#include "CSSURL.h"

#include "CSSMarkup.h"
#include "CSSParserContext.h"
#include "CSSSerializationContext.h"
#include "Document.h"

namespace WebCore {
namespace CSS {

// MARK: - Serialization

void Serialize<URL>::operator()(StringBuilder& builder, const SerializationContext& context, const URL& value)
{
    // https://drafts.csswg.org/cssom/#serialize-a-url

    builder.append(nameLiteralForSerialization(CSSValueUrl), '(');

    if (!value.resolved.isNull()) {
        if (auto replacementURLString = context.replacementURLStrings.get(value.resolved.string()); !replacementURLString.isEmpty())
            serializeString(replacementURLString, builder);
        else if (context.shouldUseResolvedURLInCSSText)
            serializeString(value.resolved.string(), builder);
        else
            serializeString(value.specified, builder);
    } else
        serializeString(value.specified, builder);

    if (value.modifiers.crossOrigin) {
        builder.append(' ');
        serializationForCSS(builder, context, *value.modifiers.crossOrigin);
    }
    if (value.modifiers.integrity) {
        builder.append(' ');
        serializationForCSS(builder, context, *value.modifiers.integrity);
    }
    if (value.modifiers.referrerPolicy) {
        builder.append(' ');
        serializationForCSS(builder, context, *value.modifiers.referrerPolicy);
    }

    builder.append(')');
}

// MARK: Operations

static URL completeURL(const String& string, const WTF::URL& baseURL)
{
    if (string.isEmpty() || string.startsWith('#'))
        return URL { .specified = string, .resolved = WTF::URL { string }, .modifiers = { } };
    if (baseURL.isNull() || baseURL.isAboutBlank())
        return URL { .specified = string, .resolved = WTF::URL { }, .modifiers = { } };
    return URL { .specified = string, .resolved = WTF::URL { baseURL, string }, .modifiers = { } };
}

std::optional<URL> completeURL(const String& string, const CSSParserContext& context)
{
    if (string.isNull())
        return { };

    auto result = completeURL(string, context.baseURL);
    if (context.mode == WebVTTMode && !result.resolved.protocolIsData())
        return { };

    result.modifiers.loadedFromOpaqueSource = context.loadedFromOpaqueSource;

    return result;
}

std::optional<URL> completeURL(const String& string, const Document& document)
{
    if (string.isNull())
        return { };
    return completeURL(string, document.baseURL());
}

URL resolve(URL&& url)
{
    if (url.resolved.isNull())
        url.resolved = WTF::URL { url.specified };
    return url;
}

bool mayDependOnBaseURL(const URL& url)
{
    return !url.specified.isEmpty()
        && !url.specified.startsWith('#')
        && !url.resolved.protocolIsData();
}

} // namespace CSS
} // namespace WebCore
