/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CorpseExportsTrie.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <mach-o/loader.h>
#include <optional>

namespace JSC {
namespace Corpse {

// The bits that Mach-O defines in an exports trie terminal's flags.
constexpr uint64_t knownExportFlagBits = EXPORT_SYMBOL_FLAGS_KIND_MASK
    | EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION
    | EXPORT_SYMBOL_FLAGS_REEXPORT
    | EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER
    | EXPORT_SYMBOL_FLAGS_STATIC_RESOLVER;

Expected<ExportsTrie::Export, ExportsTrie::Failure> ExportsTrie::lookUp(std::span<const uint8_t> trie, std::string_view name)
{
    size_t nodeOffset = 0;
    std::string_view remaining = name;

    // The only way around this loop is by matching an edge, which consumes at least
    // one character of the `remaining` name we're searching for. Because empty edges
    // are rejected below, the walk is bounded by the length of the name no matter
    // what the trie's child offsets say, and cannot be made to revisit a node forever.
    while (nodeOffset < trie.size()) {
        ByteParser node(trie, nodeOffset);

        // A terminal node in a dyld exports trie is: a length, then flags, then some
        // ULEB128s whose meaning depends on the flags. See mach-o/loader.h around lines
        // 1488–1499 for details.
        auto terminalLength = node.consumeULEB128();
        if (!terminalLength)
            return makeUnexpected(Failure::Malformed);

        // terminalLength is a full 64-bit value out of the trie, so it is compared
        // against what is left of the trie rather than by forming position + terminalLength,
        // which could wrap and pass a direct comparison. The subtraction is safe because
        // consumeULEB128 stops at the end of the trie, so the position cannot have passed it.
        if (*terminalLength > trie.size() - node.position())
            return makeUnexpected(Failure::Malformed);
        size_t childrenPosition = node.position() + *terminalLength;

        if (remaining.empty() && *terminalLength) {
            // The payload is read through a parser bounded to the terminal, so that a
            // terminal declaring less than the flags and offset it needs cannot be made
            // to take the bytes that follow it as its own.
            ByteParser terminal(trie.subspan(node.position(), *terminalLength));
            auto flags = terminal.consumeULEB128();
            if (!flags)
                return makeUnexpected(Failure::Malformed);
            if (*flags & ~knownExportFlagBits)
                return makeUnexpected(Failure::Malformed);
            if (*flags & EXPORT_SYMBOL_FLAGS_REEXPORT)
                return makeUnexpected(Failure::ReExport);

            Export::Kind kind;
            switch (*flags & EXPORT_SYMBOL_FLAGS_KIND_MASK) {
            case EXPORT_SYMBOL_FLAGS_KIND_REGULAR:
                kind = Export::Kind::Regular;
                break;
            case EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE:
                kind = Export::Kind::Absolute; // the value is the address itself.
                break;
            default:
                // Thread-local, or a kind postdating this code. A thread-local's value
                // is the offset of its TLV descriptor, not of the variable, and the
                // variable's address differs per thread, so there is no one answer to
                // report. Saying nothing beats reporting the descriptor as if it were
                // the variable.
                return makeUnexpected(Failure::UnsupportedKind);
            }

            auto value = terminal.consumeULEB128();
            if (!value)
                return makeUnexpected(Failure::Malformed);
            return Export { kind, *value };
        }

        ByteParser children(trie, childrenPosition);
        auto childCount = children.consumeByte();
        if (!childCount)
            return makeUnexpected(Failure::Malformed);

        std::optional<uint64_t> nextNodeOffset;
        for (uint8_t i = 0; i < *childCount; ++i) {
            auto edge = children.consumeCString();
            if (!edge)
                return makeUnexpected(Failure::Malformed);
            // An edge carries the characters that tell a node's children apart,
            // so an empty one is malformed. It would also match anything, and
            // descending on it would consume none of the name.
            if (edge->empty())
                return makeUnexpected(Failure::Malformed);
            auto childOffset = children.consumeULEB128();
            if (!childOffset)
                return makeUnexpected(Failure::Malformed);
            if (remaining.starts_with(*edge)) {
                remaining.remove_prefix(edge->size());
                nextNodeOffset = childOffset;
                break;
            }
        }
        // No edge matched what is left of the name, so nothing below this node
        // can hold it. A node with no children ends the walk the same way.
        if (!nextNodeOffset)
            return makeUnexpected(Failure::Absent);
        nodeOffset = *nextNodeOffset;
    }
    // A child offset led to or past the end of the trie.
    return makeUnexpected(Failure::Malformed);
}

} // namespace Corpse
} // namespace JSC

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
