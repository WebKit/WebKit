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

namespace WebKit {

void WebCompiledContentRuleListData::encode(IPC::Encoder& encoder) const
{
    SharedMemory::Handle handle;
    data->createHandle(handle, SharedMemory::Protection::ReadOnly);
    encoder << handle;

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
    SharedMemory::Handle handle;
    if (!decoder.decode(handle))
        return WTF::nullopt;
    compiledContentRuleListData.data = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);

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
