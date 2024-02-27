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

std::optional<WebCore::SharedMemoryHandle> WebCompiledContentRuleListData::createDataHandle(WebCore::SharedMemory::Protection protection) const
{
    return data->createHandle(protection);
}

WebCompiledContentRuleListData::WebCompiledContentRuleListData(String&& identifier, std::optional<WebCore::SharedMemoryHandle>&& dataHandle, size_t actionsOffset, size_t actionsSize, size_t urlFiltersBytecodeOffset, size_t urlFiltersBytecodeSize, size_t topURLFiltersBytecodeOffset, size_t topURLFiltersBytecodeSize, size_t frameURLFiltersBytecodeOffset, size_t frameURLFiltersBytecodeSize)
    : identifier(WTFMove(identifier))
    , data(dataHandle ? WebCore::SharedMemory::map(WTFMove(*dataHandle), WebCore::SharedMemory::Protection::ReadOnly) : nullptr)
    , actionsOffset(actionsOffset)
    , actionsSize(actionsSize)
    , urlFiltersBytecodeOffset(urlFiltersBytecodeOffset)
    , urlFiltersBytecodeSize(urlFiltersBytecodeSize)
    , topURLFiltersBytecodeOffset(topURLFiltersBytecodeOffset)
    , topURLFiltersBytecodeSize(topURLFiltersBytecodeSize)
    , frameURLFiltersBytecodeOffset(frameURLFiltersBytecodeOffset)
    , frameURLFiltersBytecodeSize(frameURLFiltersBytecodeSize)
{
    if (data) {
        if (data->size() < ruleListDataSize(actionsOffset, actionsSize)
        || data->size() < ruleListDataSize(urlFiltersBytecodeOffset, urlFiltersBytecodeSize)
        || data->size() < ruleListDataSize(topURLFiltersBytecodeOffset, topURLFiltersBytecodeSize)
        || data->size() < ruleListDataSize(frameURLFiltersBytecodeOffset, frameURLFiltersBytecodeSize)) {
            ASSERT_NOT_REACHED();
            data = nullptr;
        }
    }
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
