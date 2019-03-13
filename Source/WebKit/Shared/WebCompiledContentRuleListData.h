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

#pragma once

#if ENABLE(CONTENT_EXTENSIONS)

#include "NetworkCacheData.h"
#include "SharedMemory.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class WebCompiledContentRuleListData {
public:
    WebCompiledContentRuleListData() = default;

    WebCompiledContentRuleListData(Variant<RefPtr<SharedMemory>, RefPtr<WebCore::SharedBuffer>>&& data, NetworkCache::Data fileData, unsigned conditionsApplyOnlyToDomainOffset, unsigned actionsOffset, unsigned actionsSize, unsigned filtersWithoutConditionsBytecodeOffset, unsigned filtersWithoutConditionsBytecodeSize, unsigned filtersWithConditionsBytecodeOffset, unsigned filtersWithConditionsBytecodeSize, unsigned topURLFiltersBytecodeOffset, unsigned topURLFiltersBytecodeSize)
        : data(WTFMove(data))
        , fileData(fileData)
        , conditionsApplyOnlyToDomainOffset(conditionsApplyOnlyToDomainOffset)
        , actionsOffset(actionsOffset)
        , actionsSize(actionsSize)
        , filtersWithoutConditionsBytecodeOffset(filtersWithoutConditionsBytecodeOffset)
        , filtersWithoutConditionsBytecodeSize(filtersWithoutConditionsBytecodeSize)
        , filtersWithConditionsBytecodeOffset(filtersWithConditionsBytecodeOffset)
        , filtersWithConditionsBytecodeSize(filtersWithConditionsBytecodeSize)
        , topURLFiltersBytecodeOffset(topURLFiltersBytecodeOffset)
        , topURLFiltersBytecodeSize(topURLFiltersBytecodeSize)
    {
    }

    void encode(IPC::Encoder&) const;
    static Optional<WebCompiledContentRuleListData> decode(IPC::Decoder&);

    size_t size() const;
    const void* dataPointer() const;
    
    Variant<RefPtr<SharedMemory>, RefPtr<WebCore::SharedBuffer>> data;
    NetworkCache::Data fileData;
    unsigned conditionsApplyOnlyToDomainOffset { 0 };
    unsigned actionsOffset { 0 };
    unsigned actionsSize { 0 };
    unsigned filtersWithoutConditionsBytecodeOffset { 0 };
    unsigned filtersWithoutConditionsBytecodeSize { 0 };
    unsigned filtersWithConditionsBytecodeOffset { 0 };
    unsigned filtersWithConditionsBytecodeSize { 0 };
    unsigned topURLFiltersBytecodeOffset { 0 };
    unsigned topURLFiltersBytecodeSize { 0 };
};

}

#endif // ENABLE(CONTENT_EXTENSIONS)
