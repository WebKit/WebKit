/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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
#include "SecurityOrigin.h"
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

String SecurityOriginData::toString() const
{
    if (protocol == "file")
        return "file://"_s;

    if (protocol.isEmpty() && host.isEmpty())
        return { };

    if (!port)
        return makeString(protocol, "://", host);
    return makeString(protocol, "://", host, ':', static_cast<uint32_t>(*port));
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

Ref<SecurityOrigin> SecurityOriginData::securityOrigin() const
{
    return SecurityOrigin::create(protocol.isolatedCopy(), host.isolatedCopy(), port);
}

static const char separatorCharacter = '_';

String SecurityOriginData::databaseIdentifier() const
{
    // Historically, we've used the following (somewhat nonsensical) string
    // for the databaseIdentifier of local files. We used to compute this
    // string because of a bug in how we handled the scheme for file URLs.
    // Now that we've fixed that bug, we produce this string for compatibility
    // with existing persistent state.
    if (equalIgnoringASCIICase(protocol, "file"))
        return "file__0"_s;

    return makeString(protocol, separatorCharacter, FileSystem::encodeForFileName(host), separatorCharacter, port.value_or(0));
}

std::optional<SecurityOriginData> SecurityOriginData::fromDatabaseIdentifier(const String& databaseIdentifier)
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
    auto port = parseIntegerAllowingTrailingJunk<uint16_t>(StringView { databaseIdentifier }.right(portLength));

    // Nothing after the colon is fine. Failure to parse after the colon is not.
    if (!port && portLength)
        return std::nullopt;

    // Treat port 0 like there is was no port specified.
    if (port && !*port)
        port = std::nullopt;

    auto protocol = databaseIdentifier.substring(0, separator1);
    auto host = databaseIdentifier.substring(separator1 + 1, separator2 - separator1 - 1);
    return SecurityOriginData { protocol, host, port };
}

SecurityOriginData SecurityOriginData::isolatedCopy() const
{
    SecurityOriginData result;

    result.protocol = protocol.isolatedCopy();
    result.host = host.isolatedCopy();
    result.port = port;

    return result;
}

bool operator==(const SecurityOriginData& a, const SecurityOriginData& b)
{
    if (&a == &b)
        return true;

    return a.protocol == b.protocol
        && a.host == b.host
        && a.port == b.port;
}

} // namespace WebCore
