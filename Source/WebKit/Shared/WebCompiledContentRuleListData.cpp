/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebCompiledContentRuleListData.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ArgumentCoders.h"
#include "DataReference.h"

namespace WebKit {
static size_t ruleListDataSize(size_t topURLFiltersBytecodeOffset, size_t topURLFiltersBytecodeSize)
{
    return topURLFiltersBytecodeOffset + topURLFiltersBytecodeSize;
}

void WebCompiledContentRuleListData::encode(IPC::Encoder& encoder) const
{
    ASSERT(data->size() >= ruleListDataSize(topURLFiltersBytecodeOffset, topURLFiltersBytecodeSize));
    encoder << identifier;
    encoder << data->createHandle(SharedMemory::Protection::ReadOnly);
    encoder << actionsOffset;
    encoder << actionsSize;
    encoder << urlFiltersBytecodeOffset;
    encoder << urlFiltersBytecodeSize;
    encoder << topURLFiltersBytecodeOffset;
    encoder << topURLFiltersBytecodeSize;
    encoder << frameURLFiltersBytecodeOffset;
    encoder << frameURLFiltersBytecodeSize;
}

std::optional<WebCompiledContentRuleListData> WebCompiledContentRuleListData::decode(IPC::Decoder& decoder)
{
    std::optional<String> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<std::optional<SharedMemory::Handle>> handle;
    decoder >> handle;
    if (!handle)
        return std::nullopt;

    std::optional<size_t> actionsOffset;
    decoder >> actionsOffset;
    if (!actionsOffset)
        return std::nullopt;

    std::optional<size_t> actionsSize;
    decoder >> actionsSize;
    if (!actionsSize)
        return std::nullopt;

    std::optional<size_t> urlFiltersBytecodeOffset;
    decoder >> urlFiltersBytecodeOffset;
    if (!urlFiltersBytecodeOffset)
        return std::nullopt;

    std::optional<size_t> urlFiltersBytecodeSize;
    decoder >> urlFiltersBytecodeSize;
    if (!urlFiltersBytecodeSize)
        return std::nullopt;

    std::optional<size_t> topURLFiltersBytecodeOffset;
    decoder >> topURLFiltersBytecodeOffset;
    if (!topURLFiltersBytecodeOffset)
        return std::nullopt;

    std::optional<size_t> topURLFiltersBytecodeSize;
    decoder >> topURLFiltersBytecodeSize;
    if (!topURLFiltersBytecodeSize)
        return std::nullopt;

    std::optional<size_t> frameURLFiltersBytecodeOffset;
    decoder >> frameURLFiltersBytecodeOffset;
    if (!frameURLFiltersBytecodeOffset)
        return std::nullopt;

    std::optional<size_t> frameURLFiltersBytecodeSize;
    decoder >> frameURLFiltersBytecodeSize;
    if (!frameURLFiltersBytecodeSize)
        return std::nullopt;
    
    if (!*handle)
        return std::nullopt;
    auto data = SharedMemory::map(**handle, SharedMemory::Protection::ReadOnly);
    if (!data)
        return std::nullopt;
    if (data->size() < ruleListDataSize(*topURLFiltersBytecodeOffset, *topURLFiltersBytecodeSize))
        return std::nullopt;
    return {{
        WTFMove(*identifier),
        data.releaseNonNull(),
        WTFMove(*actionsOffset),
        WTFMove(*actionsSize),
        WTFMove(*urlFiltersBytecodeOffset),
        WTFMove(*urlFiltersBytecodeSize),
        WTFMove(*topURLFiltersBytecodeOffset),
        WTFMove(*topURLFiltersBytecodeSize),
        WTFMove(*frameURLFiltersBytecodeOffset),
        WTFMove(*frameURLFiltersBytecodeSize)
    }};
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
