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
#include "SerializedNFA.h"

#include "NFA.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
namespace ContentExtensions {

template<typename T>
bool writeAllToFile(FileSystem::PlatformFileHandle file, const T& container)
{
    const char* bytes = reinterpret_cast<const char*>(container.data());
    size_t bytesLength = container.size() * sizeof(container[0]);
    const char* end = bytes + bytesLength;
    while (bytes < end) {
        auto written = FileSystem::writeToFile(file, bytes, bytesLength);
        if (written == -1)
            return false;
        bytes += written;
        bytesLength -= written;
    }
    return true;
}

Optional<SerializedNFA> SerializedNFA::serialize(NFA&& nfa)
{
    auto file = FileSystem::invalidPlatformFileHandle;
    auto filename = FileSystem::openTemporaryFile("SerializedNFA", file);
    if (!FileSystem::isHandleValid(file))
        return WTF::nullopt;

    bool wroteSuccessfully = writeAllToFile(file, nfa.nodes)
        && writeAllToFile(file, nfa.transitions)
        && writeAllToFile(file, nfa.targets)
        && writeAllToFile(file, nfa.epsilonTransitionsTargets)
        && writeAllToFile(file, nfa.actions);
    if (!wroteSuccessfully) {
        FileSystem::closeFile(file);
        FileSystem::deleteFile(filename);
        return WTF::nullopt;
    }

    bool mappedSuccessfully = false;
    FileSystem::MappedFileData mappedFile(file, FileSystem::MappedFileMode::Private, mappedSuccessfully);
    FileSystem::closeFile(file);
    FileSystem::deleteFile(filename);
    if (!mappedSuccessfully)
        return WTF::nullopt;

    Metadata metadata {
        nfa.nodes.size(),
        nfa.transitions.size(),
        nfa.targets.size(),
        nfa.epsilonTransitionsTargets.size(),
        nfa.actions.size(),
        0,
        nfa.nodes.size() * sizeof(nfa.nodes[0]),
        nfa.nodes.size() * sizeof(nfa.nodes[0])
            + nfa.transitions.size() * sizeof(nfa.transitions[0]),
        nfa.nodes.size() * sizeof(nfa.nodes[0])
            + nfa.transitions.size() * sizeof(nfa.transitions[0])
            + nfa.targets.size() * sizeof(nfa.targets[0]),
        nfa.nodes.size() * sizeof(nfa.nodes[0])
            + nfa.transitions.size() * sizeof(nfa.transitions[0])
            + nfa.targets.size() * sizeof(nfa.targets[0])
            + nfa.epsilonTransitionsTargets.size() * sizeof(nfa.epsilonTransitionsTargets[0])
    };

    nfa.clear();

    return {{ WTFMove(mappedFile), WTFMove(metadata) }};
}

SerializedNFA::SerializedNFA(FileSystem::MappedFileData&& file, Metadata&& metadata)
    : m_file(WTFMove(file))
    , m_metadata(WTFMove(metadata))
{
}

template<typename T>
const T* SerializedNFA::pointerAtOffsetInFile(size_t offset) const
{
    return reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(m_file.data()) + offset);
}

auto SerializedNFA::nodes() const -> const Range<ImmutableNFANode>
{
    return { pointerAtOffsetInFile<ImmutableNFANode>(m_metadata.nodesOffset), m_metadata.nodesSize };
}

auto SerializedNFA::transitions() const -> const Range<ImmutableRange<char>>
{
    return { pointerAtOffsetInFile<ImmutableRange<char>>(m_metadata.transitionsOffset), m_metadata.transitionsSize };
}

auto SerializedNFA::targets() const -> const Range<uint32_t>
{
    return { pointerAtOffsetInFile<uint32_t>(m_metadata.targetsOffset), m_metadata.targetsSize };
}

auto SerializedNFA::epsilonTransitionsTargets() const -> const Range<uint32_t>
{
    return { pointerAtOffsetInFile<uint32_t>(m_metadata.epsilonTransitionsTargetsOffset), m_metadata.epsilonTransitionsTargetsSize };
}

auto SerializedNFA::actions() const -> const Range<uint64_t>
{
    return { pointerAtOffsetInFile<uint64_t>(m_metadata.actionsOffset), m_metadata.actionsSize };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
