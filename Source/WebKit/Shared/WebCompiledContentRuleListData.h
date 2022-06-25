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

#include "SharedMemory.h"
#include <WebCore/SharedBuffer.h>
#include <variant>
#include <wtf/RefPtr.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class WebCompiledContentRuleListData {
public:
    WebCompiledContentRuleListData(String&& identifier, Ref<SharedMemory>&& data, size_t actionsOffset, size_t actionsSize, size_t urlFiltersBytecodeOffset, size_t urlFiltersBytecodeSize, size_t topURLFiltersBytecodeOffset, size_t topURLFiltersBytecodeSize, size_t frameURLFiltersBytecodeOffset, size_t frameURLFiltersBytecodeSize)
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

    void encode(IPC::Encoder&) const;
    static std::optional<WebCompiledContentRuleListData> decode(IPC::Decoder&);

    String identifier;
    Ref<SharedMemory> data;
    size_t actionsOffset { 0 };
    size_t actionsSize { 0 };
    size_t urlFiltersBytecodeOffset { 0 };
    size_t urlFiltersBytecodeSize { 0 };
    size_t topURLFiltersBytecodeOffset { 0 };
    size_t topURLFiltersBytecodeSize { 0 };
    size_t frameURLFiltersBytecodeOffset { 0 };
    size_t frameURLFiltersBytecodeSize { 0 };
};

}

#endif // ENABLE(CONTENT_EXTENSIONS)
