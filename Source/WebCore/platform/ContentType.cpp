/*
 * Copyright (C) 2006-2021 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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
#include "ContentType.h"
#include "MIMETypeRegistry.h"
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>

namespace WebCore {

ContentType::ContentType(String&& contentType)
    : m_type(WTFMove(contentType))
{
}

ContentType::ContentType(const String& contentType)
    : m_type(contentType)
{
}

ContentType::ContentType(const String& contentType, bool typeWasInferredFromExtension)
    : m_type(contentType)
    , m_typeWasInferredFromExtension(typeWasInferredFromExtension)
{
}

ContentType ContentType::fromURL(const URL& url)
{
    ASSERT(isMainThread());

    auto lastPathComponent = url.lastPathComponent();
    size_t pos = lastPathComponent.reverseFind('.');
    if (pos != notFound) {
        auto extension = lastPathComponent.substring(pos + 1);
        String mediaType = MIMETypeRegistry::mediaMIMETypeForExtension(extension);
        if (!mediaType.isEmpty())
            return ContentType(WTFMove(mediaType), true);
    }
    return ContentType();
}

const String& ContentType::codecsParameter()
{
    static NeverDestroyed<String> codecs { "codecs"_s };
    return codecs;
}

const String& ContentType::profilesParameter()
{
    static NeverDestroyed<String> profiles { "profiles"_s };
    return profiles;
}

String ContentType::parameter(const String& parameterName) const
{
    // A MIME type can have one or more "param=value" after a semicolon, separated from each other by semicolons.

    // FIXME: This will ignore a quotation mark if it comes before the semicolon. Is that the desired behavior?
    auto semicolonPosition = m_type.find(';');
    if (semicolonPosition == notFound)
        return { };

    // FIXME: This matches parameters that have parameterName as a suffix; that is not the desired behavior.
    auto nameStart = m_type.findIgnoringASCIICase(parameterName, semicolonPosition + 1);
    if (nameStart == notFound)
        return { };

    auto equalSignPosition = m_type.find('=', nameStart + parameterName.length());
    if (equalSignPosition == notFound)
        return { };

    // FIXME: This skips over any characters that come before a quotation mark; that is not the desired behavior.
    auto quotePosition = m_type.find('"', equalSignPosition + 1);
    // FIXME: This does not work if there is an escaped quotation mark in the quoted string. Is that the desired behavior?
    auto secondQuotePosition = m_type.find('"', quotePosition + 1);
    size_t start;
    size_t end;
    if (quotePosition != notFound && secondQuotePosition != notFound) {
        start = quotePosition + 1;
        end = secondQuotePosition;
    } else {
        // FIXME: If there is only one quotation mark, this will treat it as part of the string; that is not the desired behavior.
        start = equalSignPosition + 1;
        end = m_type.find(';', start);
    }
    return StringView { m_type }.substring(start, end - start).trim(isASCIIWhitespace<UChar>).toString();
}

String ContentType::containerType() const
{
    // Strip parameters that come after a semicolon.
    // FIXME: This will ignore a quotation mark if it comes before the semicolon. Is that the desired behavior?
    return m_type.left(m_type.find(';')).trim(isASCIIWhitespace);
}

static inline Vector<String> splitParameters(StringView parametersView)
{
    Vector<String> result;
    for (auto view : parametersView.split(','))
        result.append(view.trim(isASCIIWhitespace<UChar>).toString());
    return result;
}

Vector<String> ContentType::codecs() const
{
    return splitParameters(parameter(codecsParameter()));
}

Vector<String> ContentType::profiles() const
{
    return splitParameters(parameter(profilesParameter()));
}

String ContentType::toJSONString() const
{
    auto object = JSON::Object::create();

    object->setString("containerType"_s, containerType());

    auto codecs = codecsParameter();
    if (!codecs.isEmpty())
        object->setString("codecs"_s, codecs);

    auto profiles = profilesParameter();
    if (!profiles.isEmpty())
        object->setString("profiles"_s, profiles);

    return object->toJSONString();
}

} // namespace WebCore
