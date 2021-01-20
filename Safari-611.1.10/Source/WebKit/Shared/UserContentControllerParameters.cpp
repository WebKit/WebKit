/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "UserContentControllerParameters.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"

namespace WebKit {

void UserContentControllerParameters::encode(IPC::Encoder& encoder) const
{
    encoder << identifier;
    encoder << userContentWorlds;
    encoder << userScripts;
    encoder << userStyleSheets;
    encoder << messageHandlers;
#if ENABLE(CONTENT_EXTENSIONS)
    encoder << contentRuleLists;
#endif
}

Optional<UserContentControllerParameters> UserContentControllerParameters::decode(IPC::Decoder& decoder)
{
    Optional<UserContentControllerIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return WTF::nullopt;
    
    Optional<Vector<std::pair<ContentWorldIdentifier, String>>> userContentWorlds;
    decoder >> userContentWorlds;
    if (!userContentWorlds)
        return WTF::nullopt;

    Optional<Vector<WebUserScriptData>> userScripts;
    decoder >> userScripts;
    if (!userScripts)
        return WTF::nullopt;
    
    Optional<Vector<WebUserStyleSheetData>> userStyleSheets;
    decoder >> userStyleSheets;
    if (!userStyleSheets)
        return WTF::nullopt;
    
    Optional<Vector<WebScriptMessageHandlerData>> messageHandlers;
    decoder >> messageHandlers;
    if (!messageHandlers)
        return WTF::nullopt;
    
#if ENABLE(CONTENT_EXTENSIONS)
    Optional<Vector<std::pair<String, WebCompiledContentRuleListData>>> contentRuleLists;
    decoder >> contentRuleLists;
    if (!contentRuleLists)
        return WTF::nullopt;
#endif

    return {{
        WTFMove(*identifier),
        WTFMove(*userContentWorlds),
        WTFMove(*userScripts),
        WTFMove(*userStyleSheets),
        WTFMove(*messageHandlers),
#if ENABLE(CONTENT_EXTENSIONS)
        WTFMove(*contentRuleLists),
#endif
    }};
}
    
} // namespace WebKit
