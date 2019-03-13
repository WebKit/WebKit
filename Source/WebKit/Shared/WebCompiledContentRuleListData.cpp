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
#include "SharedBufferDataReference.h"

namespace WebKit {

size_t WebCompiledContentRuleListData::size() const
{
    return WTF::switchOn(data, [] (const auto& sharedMemoryOrBuffer) {
        return sharedMemoryOrBuffer->size();
    });
}

const void* WebCompiledContentRuleListData::dataPointer() const
{
    return WTF::switchOn(data, [] (const auto& sharedMemoryOrBuffer) -> const void* {
        return sharedMemoryOrBuffer->data();
    });
}

void WebCompiledContentRuleListData::encode(IPC::Encoder& encoder) const
{
    if (auto sharedMemory = WTF::get_if<RefPtr<SharedMemory>>(data)) {
        encoder << true;
        SharedMemory::Handle handle;
        sharedMemory->get()->createHandle(handle, SharedMemory::Protection::ReadOnly);
        encoder << handle;
    } else {
        encoder << false;
        encoder << IPC::SharedBufferDataReference { *WTF::get<RefPtr<WebCore::SharedBuffer>>(data) };
    }

    // fileData needs to be kept in the UIProcess, but it does not need to be serialized.
    // FIXME: Move it to API::ContentRuleList

    encoder << conditionsApplyOnlyToDomainOffset;
    encoder << actionsOffset;
    encoder << actionsSize;
    encoder << filtersWithoutConditionsBytecodeOffset;
    encoder << filtersWithoutConditionsBytecodeSize;
    encoder << filtersWithConditionsBytecodeOffset;
    encoder << filtersWithConditionsBytecodeSize;
    encoder << topURLFiltersBytecodeOffset;
    encoder << topURLFiltersBytecodeSize;
}

Optional<WebCompiledContentRuleListData> WebCompiledContentRuleListData::decode(IPC::Decoder& decoder)
{
    WebCompiledContentRuleListData compiledContentRuleListData;

    Optional<bool> hasSharedMemory;
    decoder >> hasSharedMemory;
    if (!hasSharedMemory)
        return WTF::nullopt;
    if (*hasSharedMemory) {
        SharedMemory::Handle handle;
        if (!decoder.decode(handle))
            return WTF::nullopt;
        compiledContentRuleListData.data = { SharedMemory::map(handle, SharedMemory::Protection::ReadOnly) };
    } else {
        IPC::DataReference dataReference;
        if (!decoder.decode(dataReference))
            return WTF::nullopt;
        compiledContentRuleListData.data = { RefPtr<WebCore::SharedBuffer>(WebCore::SharedBuffer::create(dataReference.data(), dataReference.size())) };
    }

    if (!decoder.decode(compiledContentRuleListData.conditionsApplyOnlyToDomainOffset))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.actionsOffset))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.actionsSize))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.filtersWithoutConditionsBytecodeOffset))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.filtersWithoutConditionsBytecodeSize))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.filtersWithConditionsBytecodeOffset))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.filtersWithConditionsBytecodeSize))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.topURLFiltersBytecodeOffset))
        return WTF::nullopt;
    if (!decoder.decode(compiledContentRuleListData.topURLFiltersBytecodeSize))
        return WTF::nullopt;

    return WTFMove(compiledContentRuleListData);
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
