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

#include <WebCore/SharedBuffer.h>
#include <WebCore/SharedMemory.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebCompiledContentRuleListData {
public:
    WebCompiledContentRuleListData(String&& identifier, Ref<WebCore::SharedMemory>&& data, uint64_t actionsOffset, uint64_t actionsSize, uint64_t urlFiltersBytecodeOffset, uint64_t urlFiltersBytecodeSize, uint64_t topURLFiltersBytecodeOffset, uint64_t topURLFiltersBytecodeSize, uint64_t frameURLFiltersBytecodeOffset, uint64_t frameURLFiltersBytecodeSize)
        : identifier(WTFMove(identifier))
        , data(WTFMove(data))
        , actionsOffset(actionsOffset)
        , actionsSize(actionsSize)
        , urlFiltersBytecodeOffset(urlFiltersBytecodeOffset)
        , urlFiltersBytecodeSize(urlFiltersBytecodeSize)
        , topURLFiltersBytecodeOffset(topURLFiltersBytecodeOffset)
        , topURLFiltersBytecodeSize(topURLFiltersBytecodeSize)
        , frameURLFiltersBytecodeOffset(frameURLFiltersBytecodeOffset)
        , frameURLFiltersBytecodeSize(frameURLFiltersBytecodeSize)
    {
    }

    WebCompiledContentRuleListData(String&& identifier, std::optional<WebCore::SharedMemoryHandle>&& data, uint64_t actionsOffset, uint64_t actionsSize, uint64_t urlFiltersBytecodeOffset, uint64_t urlFiltersBytecodeSize, uint64_t topURLFiltersBytecodeOffset, uint64_t topURLFiltersBytecodeSize, uint64_t frameURLFiltersBytecodeOffset, uint64_t frameURLFiltersBytecodeSize);

    std::optional<WebCore::SharedMemoryHandle> createDataHandle(WebCore::SharedMemory::Protection = WebCore::SharedMemory::Protection::ReadOnly) const;

    String identifier;
    RefPtr<WebCore::SharedMemory> data;
    uint64_t actionsOffset { 0 };
    uint64_t actionsSize { 0 };
    uint64_t urlFiltersBytecodeOffset { 0 };
    uint64_t urlFiltersBytecodeSize { 0 };
    uint64_t topURLFiltersBytecodeOffset { 0 };
    uint64_t topURLFiltersBytecodeSize { 0 };
    uint64_t frameURLFiltersBytecodeOffset { 0 };
    uint64_t frameURLFiltersBytecodeSize { 0 };
};

}

#endif // ENABLE(CONTENT_EXTENSIONS)
