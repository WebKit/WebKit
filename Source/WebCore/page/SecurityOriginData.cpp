/*
 * Copyright (C) 2011, 2015, 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SecurityOriginData.h"

#include "Document.h"
#include "Frame.h"
#include "LegacySchemeRegistry.h"
#include "SecurityOrigin.h"
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

String SecurityOriginData::toString() const
{
    auto protocol = this->protocol();
    if (protocol == "file"_s)
        return "file://"_s;

    auto host = this->host();
    if (protocol.isEmpty() && host.isEmpty())
        return { };

    auto port = this->port();
    if (!port)
        return makeString(protocol, "://", host);
    return makeString(protocol, "://", host, ':', static_cast<uint32_t>(*port));
}

URL SecurityOriginData::toURL() const
{
    return URL { toString() };
}

SecurityOriginData SecurityOriginData::fromFrame(Frame* frame)
{
    if (!frame)
        return SecurityOriginData { };
    
    auto* document = frame->document();
    if (!document)
        return SecurityOriginData { };

    return document->securityOrigin().data();
}

SecurityOriginData SecurityOriginData::fromURL(const URL& url)
{
    if (shouldTreatAsOpaqueOrigin(url))
        return createOpaque();
    return fromURLWithoutStrictOpaqueness(url);
}

SecurityOriginData SecurityOriginData::fromURLWithoutStrictOpaqueness(const URL& url)
{
    if (url.isNull())
        return SecurityOriginData { };
    if (url.protocol().isEmpty() && url.host().isEmpty() && !url.port())
        return createOpaque();
    return SecurityOriginData {
        url.protocol().isNull() ? emptyString() : url.protocol().convertToASCIILowercase()
        , url.host().isNull() ? emptyString() : url.host().convertToASCIILowercase()
        , url.port()
    };
}

Ref<SecurityOrigin> SecurityOriginData::securityOrigin() const
{
    return SecurityOrigin::create(isolatedCopy());
}

static const char separatorCharacter = '_';

String SecurityOriginData::databaseIdentifier() const
{
    // Historically, we've used the following (somewhat nonsensical) string
    // for the databaseIdentifier of local files. We used to compute this
    // string because of a bug in how we handled the scheme for file URLs.
    // Now that we've fixed that bug, we produce this string for compatibility
    // with existing persistent state.
    auto protocol = this->protocol();
    if (equalLettersIgnoringASCIICase(protocol, "file"_s))
        return "file__0"_s;

    return makeString(protocol, separatorCharacter, FileSystem::encodeForFileName(host()), separatorCharacter, port().value_or(0));
}

std::optional<SecurityOriginData> SecurityOriginData::fromDatabaseIdentifier(StringView databaseIdentifier)
{
    // Make sure there's a first separator
    size_t separator1 = databaseIdentifier.find(separatorCharacter);
    if (separator1 == notFound)
        return std::nullopt;
    
    // Make sure there's a second separator
    size_t separator2 = databaseIdentifier.reverseFind(separatorCharacter);
    if (separator2 == notFound)
        return std::nullopt;
    
    // Ensure there were at least 2 separator characters. Some hostnames on intranets have
    // underscores in them, so we'll assume that any additional underscores are part of the host.
    if (separator1 == separator2)
        return std::nullopt;
    
    // Make sure the port section is a valid port number or doesn't exist.
    auto portLength = databaseIdentifier.length() - separator2 - 1;
    auto port = parseIntegerAllowingTrailingJunk<uint16_t>(databaseIdentifier.right(portLength));

    // Nothing after the colon is fine. Failure to parse after the colon is not.
    if (!port && portLength)
        return std::nullopt;

    // Treat port 0 like there is was no port specified.
    if (port && !*port)
        port = std::nullopt;

    auto protocol = databaseIdentifier.left(separator1);
    auto host = databaseIdentifier.substring(separator1 + 1, separator2 - separator1 - 1);
    return SecurityOriginData { protocol.toString(), host.toString(), port };
}

SecurityOriginData SecurityOriginData::isolatedCopy() const &
{
    return SecurityOriginData { crossThreadCopy(m_data) };
}

SecurityOriginData SecurityOriginData::isolatedCopy() &&
{
    return SecurityOriginData { crossThreadCopy(WTFMove(m_data)) };
}

bool operator==(const SecurityOriginData& a, const SecurityOriginData& b)
{
    if (&a == &b)
        return true;

    return a.data() == b.data();
}

static bool schemeRequiresHost(const URL& url)
{
    // We expect URLs with these schemes to have authority components. If the
    // URL lacks an authority component, we get concerned and mark the origin
    // as opaque.
    return url.protocolIsInHTTPFamily() || url.protocolIs("ftp"_s);
}

bool SecurityOriginData::shouldTreatAsOpaqueOrigin(const URL& url)
{
    if (!url.isValid())
        return true;

    // FIXME: Do we need to unwrap the URL further?
    URL innerURL = SecurityOrigin::shouldUseInnerURL(url) ? SecurityOrigin::extractInnerURL(url) : url;
    if (!innerURL.isValid())
        return true;

    // For edge case URLs that were probably misparsed, make sure that the origin is opaque.
    // This is an additional safety net against bugs in URL parsing, and for network back-ends that parse URLs differently,
    // and could misinterpret another component for hostname.
    if (schemeRequiresHost(innerURL) && innerURL.host().isEmpty())
        return true;

    if (LegacySchemeRegistry::shouldTreatURLSchemeAsNoAccess(innerURL.protocol()))
        return true;

    // https://url.spec.whatwg.org/#origin with some additions
    if (url.hasSpecialScheme()
#if PLATFORM(COCOA)
        || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NullOriginForNonSpecialSchemedURLs)
        || url.protocolIs("applewebdata"_s)
        || url.protocolIs("x-apple-ql-id"_s)
        || url.protocolIs("x-apple-ql-id2"_s)
        || url.protocolIs("x-apple-ql-magic"_s)
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
        || url.protocolIs("resource"_s)
#endif
#if ENABLE(PDFJS)
        || url.protocolIs("webkit-pdfjs-viewer"_s)
#endif
        || url.protocolIs("blob"_s))
        return false;

    return !LegacySchemeRegistry::schemeIsHandledBySchemeHandler(url.protocol());
}

} // namespace WebCore
