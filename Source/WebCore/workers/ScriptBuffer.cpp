/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptBuffer.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
static std::optional<ShareableResource::Handle> tryConvertToShareableResourceHandle(const ScriptBuffer& script)
{
    if (!script.containsSingleFileMappedSegment())
        return std::nullopt;

    auto& segment = script.buffer()->begin()->segment;
    auto sharedMemory = SharedMemory::wrapMap(const_cast<uint8_t*>(segment->data()), segment->size(), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return std::nullopt;

    auto shareableResource = ShareableResource::create(sharedMemory.releaseNonNull(), 0, segment->size());
    if (!shareableResource)
        return std::nullopt;

    return shareableResource->createHandle();
}
#endif

ScriptBuffer::ScriptBuffer(const String& string)
{
    append(string);
}

ScriptBuffer ScriptBuffer::empty()
{
    return ScriptBuffer { SharedBuffer::create() };
}

String ScriptBuffer::toString() const
{
    if (!m_buffer)
        return String();

    StringBuilder builder;
    m_buffer.get()->forEachSegment([&](auto& segment) {
        builder.append(String::fromUTF8(segment.data(), segment.size()));
    });
    return builder.toString();
}

bool ScriptBuffer::containsSingleFileMappedSegment() const
{
    return m_buffer && m_buffer.get()->hasOneSegment() && m_buffer.get()->begin()->segment->containsMappedFileData();
}

void ScriptBuffer::append(const String& string)
{
    if (string.isEmpty())
        return;
    auto result = string.tryGetUTF8([&](std::span<const char> span) -> bool {
        m_buffer.append(span.data(), span.size());
        return true;
    });
    RELEASE_ASSERT(result);
}

void ScriptBuffer::append(const FragmentedSharedBuffer& buffer)
{
    m_buffer.append(buffer);
}

std::optional<ScriptBuffer> ScriptBuffer::fromIPCData(IPCData&& ipcData)
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    return WTF::switchOn(WTFMove(ipcData), [](ShareableResourceHandle&& handle) -> std::optional<ScriptBuffer> {
        if (RefPtr buffer = WTFMove(handle).tryWrapInSharedBuffer())
            return ScriptBuffer { WTFMove(buffer) };
        return std::nullopt;
    }, [](RefPtr<FragmentedSharedBuffer>&& buffer) -> std::optional<ScriptBuffer> {
        return ScriptBuffer { WTFMove(buffer) };
    });
#else
    return ScriptBuffer { WTFMove(ipcData) };
#endif
}

auto ScriptBuffer::ipcData() const -> IPCData
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    if (auto handle = tryConvertToShareableResourceHandle(*this))
        return { WTFMove(*handle) };
#endif
    return m_buffer.get();
}

bool operator==(const ScriptBuffer& a, const ScriptBuffer& b)
{
    if (a.buffer() == b.buffer())
        return true;
    if (!a.buffer() || !b.buffer())
        return false;
    return *a.buffer() == *b.buffer();
}

} // namespace WebCore
