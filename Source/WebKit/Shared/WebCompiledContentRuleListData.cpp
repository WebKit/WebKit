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
#include "SharedBufferDataReference.h"

namespace WebKit {

void WebCompiledContentRuleListData::encode(IPC::Encoder& encoder) const
{
    SharedMemory::Handle handle;
    data->createHandle(handle, SharedMemory::Protection::ReadOnly);
    
#if OS(DARWIN) || OS(WINDOWS)
    // Exact data size is the last bytecode offset plus its size.
    uint64_t dataSize = topURLFiltersBytecodeOffset + topURLFiltersBytecodeSize;
#else
    uint64_t dataSize = 0;
#endif
    encoder << SharedMemory::IPCHandle { WTFMove(handle), dataSize };

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
    SharedMemory::IPCHandle ipcHandle;
    if (!decoder.decode(ipcHandle))
        return WTF::nullopt;
    RefPtr<SharedMemory> data = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);

    Optional<unsigned> conditionsApplyOnlyToDomainOffset;
    decoder >> conditionsApplyOnlyToDomainOffset;
    if (!conditionsApplyOnlyToDomainOffset)
        return WTF::nullopt;

    Optional<unsigned> actionsOffset;
    decoder >> actionsOffset;
    if (!actionsOffset)
        return WTF::nullopt;

    Optional<unsigned> actionsSize;
    decoder >> actionsSize;
    if (!actionsSize)
        return WTF::nullopt;

    Optional<unsigned> filtersWithoutConditionsBytecodeOffset;
    decoder >> filtersWithoutConditionsBytecodeOffset;
    if (!filtersWithoutConditionsBytecodeOffset)
        return WTF::nullopt;

    Optional<unsigned> filtersWithoutConditionsBytecodeSize;
    decoder >> filtersWithoutConditionsBytecodeSize;
    if (!filtersWithoutConditionsBytecodeSize)
        return WTF::nullopt;

    Optional<unsigned> filtersWithConditionsBytecodeOffset;
    decoder >> filtersWithConditionsBytecodeOffset;
    if (!filtersWithConditionsBytecodeOffset)
        return WTF::nullopt;

    Optional<unsigned> filtersWithConditionsBytecodeSize;
    decoder >> filtersWithConditionsBytecodeSize;
    if (!filtersWithConditionsBytecodeSize)
        return WTF::nullopt;

    Optional<unsigned> topURLFiltersBytecodeOffset;
    decoder >> topURLFiltersBytecodeOffset;
    if (!topURLFiltersBytecodeOffset)
        return WTF::nullopt;

    Optional<unsigned> topURLFiltersBytecodeSize;
    decoder >> topURLFiltersBytecodeSize;
    if (!topURLFiltersBytecodeSize)
        return WTF::nullopt;

    return {{
        WTFMove(data),
        WTFMove(*conditionsApplyOnlyToDomainOffset),
        WTFMove(*actionsOffset),
        WTFMove(*actionsSize),
        WTFMove(*filtersWithoutConditionsBytecodeOffset),
        WTFMove(*filtersWithoutConditionsBytecodeSize),
        WTFMove(*filtersWithConditionsBytecodeOffset),
        WTFMove(*filtersWithConditionsBytecodeSize),
        WTFMove(*topURLFiltersBytecodeOffset),
        WTFMove(*topURLFiltersBytecodeSize)
    }};
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
