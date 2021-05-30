/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ServiceWorkerClientIdentifier.h"

#if ENABLE(SERVICE_WORKER)

#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

std::optional<ServiceWorkerClientIdentifier> ServiceWorkerClientIdentifier::fromString(StringView string)
{
    ServiceWorkerClientIdentifier clientIdentifier;
    unsigned counter = 0;
    for (auto item : string.split('-')) {
        auto identifier = parseInteger<uint64_t>(item);
        if (!identifier || !*identifier)
            return std::nullopt;
        if (!counter++)
            clientIdentifier.serverConnectionIdentifier = makeObjectIdentifier<SWServerConnectionIdentifierType>(identifier.value());
        else if (counter == 2)
            clientIdentifier.contextIdentifier = makeObjectIdentifier<DocumentIdentifierType>(identifier.value());
    }
    return (counter == 2) ? std::make_optional(WTFMove(clientIdentifier)) : std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
