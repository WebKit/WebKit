/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebUserContentControllerDataTypes.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

void WebUserScriptData::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << worldIdentifier;
    encoder << userScript;
}

std::optional<WebUserScriptData> WebUserScriptData::decode(IPC::Decoder& decoder)
{
    std::optional<uint64_t> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;
    
    std::optional<ContentWorldIdentifier> worldIdentifier;
    decoder >> worldIdentifier;
    if (!worldIdentifier)
        return std::nullopt;
    
    std::optional<WebCore::UserScript> userScript;
    decoder >> userScript;
    if (!userScript)
        return std::nullopt;
    
    return {{ WTFMove(*identifier), WTFMove(*worldIdentifier), WTFMove(*userScript) }};
}

void WebUserStyleSheetData::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << worldIdentifier;
    encoder << userStyleSheet;
}

std::optional<WebUserStyleSheetData> WebUserStyleSheetData::decode(IPC::Decoder& decoder)
{
    std::optional<uint64_t> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;
    
    std::optional<ContentWorldIdentifier> worldIdentifier;
    decoder >> worldIdentifier;
    if (!worldIdentifier)
        return std::nullopt;
    
    WebCore::UserStyleSheet userStyleSheet;
    if (!decoder.decode(userStyleSheet))
        return std::nullopt;
    
    return {{ WTFMove(*identifier), WTFMove(*worldIdentifier), WTFMove(userStyleSheet) }};
}


void WebScriptMessageHandlerData::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << worldIdentifier;
    encoder << name;
}

std::optional<WebScriptMessageHandlerData> WebScriptMessageHandlerData::decode(IPC::Decoder& decoder)
{
    std::optional<uint64_t> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;
    
    std::optional<ContentWorldIdentifier> worldIdentifier;
    decoder >> worldIdentifier;
    if (!worldIdentifier)
        return std::nullopt;
    
    std::optional<AtomString> name;
    decoder >> name;
    if (!name)
        return std::nullopt;

    return {{ WTFMove(*identifier), WTFMove(*worldIdentifier), WTFMove(*name) }};
}

} // namespace WebKit
