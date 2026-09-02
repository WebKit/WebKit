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
#include "CorpseExportsTrieTest.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseExportsTrie.h>
#include <array>
#include <limits>
#include <mach-o/loader.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <wtf/Atomics.h>

namespace JSCToolsTest {

using JSC::Corpse::ExportsTrie;

namespace {

// Builds the bytes of a dyld exports trie, or of something that is not quite one.
// Emitting bytes rather than describing exports is deliberate: most of what is
// worth testing here is malformed, and could not be described any other way.
class TrieBytes {
public:
    void byte(uint8_t value) { m_bytes.append(value); }
    void bytes(std::span<const uint8_t>);
    void uleb128(uint64_t);

    // A ULEB128 padded to a fixed width so that a forward reference can be
    // patched once its target is known. Non-canonical but well formed: the
    // padding bytes carry a continuation bit and no payload.
    static constexpr unsigned fixedWidth = 3;
    size_t uleb128Fixed(uint64_t);
    void patchUleb128Fixed(size_t position, uint64_t);

    void cString(std::string_view); // With its terminator.
    void string(std::string_view); // Without.

    size_t position() const { return m_bytes.size(); }
    std::span<const uint8_t> span() const { return m_bytes.span(); }
    Vector<uint8_t> take() { return WTF::move(m_bytes); }

private:
    Vector<uint8_t> m_bytes;
};

// The flags and payload of one terminal, as a trie encodes them.
struct TerminalSpec {
    uint64_t flags { 0 };
    // The ULEB128s that follow the flags. What they mean depends on the flags:
    // one offset for an ordinary export, a stub offset then a resolver offset
    // for a stub-and-resolver, an ordinal for a re-export.
    Vector<uint64_t> values;
    // Appended after the values, for a re-export's imported name.
    std::string_view trailingString;
    bool hasTrailingString { false };
};

// A trie holding one export named "", so that a look up of "" reaches the
// terminal without walking any edge. Isolates terminal decoding from the walk.
Vector<uint8_t> terminalOnlyTrie(const TerminalSpec&);

// A trie whose root has one edge, `name`, leading to a terminal.
Vector<uint8_t> singleExportTrie(std::string_view name, const TerminalSpec&);

// A trie spelling one export across several edges, so a look up has to walk.
Vector<uint8_t> chainedExportTrie(std::span<const std::string_view> edges, const TerminalSpec&);

// An ordinary export at `offset` from the image's base.
TerminalSpec regularExport(uint64_t offset);

void TrieBytes::bytes(std::span<const uint8_t> data)
{
    m_bytes.append(data);
}

void TrieBytes::uleb128(uint64_t value)
{
    do {
        uint8_t group = value & 0x7f;
        value >>= 7;
        if (value)
            group |= 0x80;
        m_bytes.append(group);
    } while (value);
}

size_t TrieBytes::uleb128Fixed(uint64_t value)
{
    size_t start = m_bytes.size();
    for (unsigned i = 0; i < fixedWidth; ++i) {
        uint8_t group = (value >> (7 * i)) & 0x7f;
        if (i + 1 < fixedWidth)
            group |= 0x80;
        m_bytes.append(group);
    }
    return start;
}

void TrieBytes::patchUleb128Fixed(size_t position, uint64_t value)
{
    for (unsigned i = 0; i < fixedWidth; ++i) {
        uint8_t group = (value >> (7 * i)) & 0x7f;
        if (i + 1 < fixedWidth)
            group |= 0x80;
        m_bytes[position + i] = group;
    }
}

void TrieBytes::cString(std::string_view text)
{
    string(text);
    m_bytes.append(0);
}

void TrieBytes::string(std::string_view text)
{
    for (char character : text)
        m_bytes.append(static_cast<uint8_t>(character));
}

TerminalSpec regularExport(uint64_t offset)
{
    TerminalSpec spec;
    spec.values.append(offset);
    return spec;
}

// The bytes a terminal node holds after its length: the flags, then whatever
// ULEB128s and string the flags call for.
static Vector<uint8_t> terminalPayload(const TerminalSpec& spec)
{
    TrieBytes payload;
    payload.uleb128(spec.flags);
    for (uint64_t value : spec.values)
        payload.uleb128(value);
    if (spec.hasTrailingString)
        payload.cString(spec.trailingString);
    return payload.take();
}

// A node with a terminal and no children.
static void appendTerminalNode(TrieBytes& trie, const TerminalSpec& spec)
{
    Vector<uint8_t> payload = terminalPayload(spec);
    trie.uleb128(payload.size());
    trie.bytes(payload.span());
    trie.byte(0); // No children.
}

Vector<uint8_t> terminalOnlyTrie(const TerminalSpec& spec)
{
    TrieBytes trie;
    appendTerminalNode(trie, spec);
    return trie.take();
}

Vector<uint8_t> chainedExportTrie(std::span<const std::string_view> edges, const TerminalSpec& spec)
{
    TrieBytes trie;

    // Every node but the last points at the one after it, whose position is not
    // known until it has been emitted, so each reference is patched from behind.
    Vector<size_t> patchPositions;
    for (std::string_view edge : edges) {
        if (!patchPositions.isEmpty()) {
            trie.patchUleb128Fixed(patchPositions.last(), trie.position());
            patchPositions.removeLast();
        }
        trie.uleb128(0); // No terminal on the way down.
        trie.byte(1); // One edge.
        trie.cString(edge);
        patchPositions.append(trie.uleb128Fixed(0));
    }
    if (!patchPositions.isEmpty())
        trie.patchUleb128Fixed(patchPositions.last(), trie.position());
    appendTerminalNode(trie, spec);
    return trie.take();
}

Vector<uint8_t> singleExportTrie(std::string_view name, const TerminalSpec& spec)
{
    std::array<std::string_view, 1> edges { name };
    return chainedExportTrie(std::span<const std::string_view>(edges), spec);
}

// A node's children count is one byte, so 255 edges is as wide as a node can be.
static constexpr unsigned maximumEdgeCount = 255;

// The name of the `index`th edge of a fan-out trie. Two characters wide so that no
// edge is a prefix of another, which leaves exactly one of them matching a name.
static std::string fanOutEdgeName(unsigned index)
{
    return { static_cast<char>('a' + index / 16), static_cast<char>('a' + index % 16) };
}

// Distinct per edge, so that a walk landing on the wrong child is caught.
static constexpr uint64_t fanOutExportOffset(unsigned index)
{
    return 0x1000 + index;
}

// A root with `edgeCount` edges, each leading to a terminal of its own.
static Vector<uint8_t> fanOutTrie(unsigned edgeCount)
{
    TrieBytes trie;
    trie.uleb128(0); // The root itself exports nothing.
    trie.byte(static_cast<uint8_t>(edgeCount));

    // Each edge points at a node that has not been emitted yet, so the references
    // are patched once their targets are laid down below.
    Vector<size_t> patchPositions;
    for (unsigned index = 0; index < edgeCount; ++index) {
        trie.cString(fanOutEdgeName(index));
        patchPositions.append(trie.uleb128Fixed(0));
    }
    for (unsigned index = 0; index < edgeCount; ++index) {
        trie.patchUleb128Fixed(patchPositions[index], trie.position());
        appendTerminalNode(trie, regularExport(fanOutExportOffset(index)));
    }
    return trie.take();
}

using Failure = ExportsTrie::Failure;
using Kind = ExportsTrie::Export::Kind;

} // anonymous namespace

// A node carrying both a terminal and one edge, which is how a trie holds an
// export whose name is a prefix of another export's name.
static Vector<uint8_t> prefixAndChildTrie(std::string_view rootEdge, const TerminalSpec& atRootEdge,
    std::string_view childEdge, const TerminalSpec& atChild)
{
    TrieBytes trie;
    trie.uleb128(0); // The root itself exports nothing.
    trie.byte(1);
    trie.cString(rootEdge);
    size_t rootEdgePatch = trie.uleb128Fixed(0);

    trie.patchUleb128Fixed(rootEdgePatch, trie.position());
    TrieBytes payload;
    payload.uleb128(atRootEdge.flags);
    for (uint64_t value : atRootEdge.values)
        payload.uleb128(value);
    Vector<uint8_t> payloadBytes = payload.take();
    trie.uleb128(payloadBytes.size());
    trie.bytes(payloadBytes.span());
    trie.byte(1);
    trie.cString(childEdge);
    size_t childPatch = trie.uleb128Fixed(0);

    trie.patchUleb128Fixed(childPatch, trie.position());
    TrieBytes childPayload;
    childPayload.uleb128(atChild.flags);
    for (uint64_t value : atChild.values)
        childPayload.uleb128(value);
    Vector<uint8_t> childPayloadBytes = childPayload.take();
    trie.uleb128(childPayloadBytes.size());
    trie.bytes(childPayloadBytes.span());
    trie.byte(0);

    return trie.take();
}

static void testFoundExports()
{
    {
        Vector<uint8_t> trie = singleExportTrie("_foo", regularExport(0x1234));
        auto found = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(found, "an exported name is found");
        if (found) {
            TEST_ASSERT(found->kind == Kind::Regular, "an ordinary export is Regular");
            TEST_ASSERT_HEX_EQ(found->value, 0x1234, "an ordinary export yields its offset");
        }
    }
    {
        TerminalSpec spec;
        spec.flags = EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE;
        spec.values.append(0xdeadbeef);
        Vector<uint8_t> trie = singleExportTrie("_absolute", spec);
        auto found = ExportsTrie::lookUp(trie.span(), "_absolute");
        TEST_ASSERT(found, "an absolute export is found");
        if (found) {
            TEST_ASSERT(found->kind == Kind::Absolute, "an absolute export is Absolute");
            TEST_ASSERT_HEX_EQ(found->value, 0xdeadbeef, "an absolute export yields the address itself");
        }
    }
    {
        // A weak definition is still an ordinary export; the flag sits outside the
        // kind mask and must not disturb it.
        TerminalSpec spec;
        spec.flags = EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION | EXPORT_SYMBOL_FLAGS_KIND_REGULAR;
        spec.values.append(0x40);
        Vector<uint8_t> trie = singleExportTrie("_weak", spec);
        auto found = ExportsTrie::lookUp(trie.span(), "_weak");
        TEST_ASSERT(found, "a weak definition is found");
        if (found) {
            TEST_ASSERT(found->kind == Kind::Regular, "a weak definition is Regular");
            TEST_ASSERT_HEX_EQ(found->value, 0x40, "a weak definition yields its offset");
        }
    }
    {
        // Per <mach-o/loader.h>, a stub-and-resolver terminal holds two ULEB128s:
        // the stub offset and then the resolver offset. The stub is the address
        // the symbol resolves to; the resolver is only how a lazy binding finds it.
        TerminalSpec spec;
        spec.flags = EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER | EXPORT_SYMBOL_FLAGS_KIND_REGULAR;
        spec.values.append(0x1000); // Stub offset.
        spec.values.append(0x2000); // Resolver offset.
        Vector<uint8_t> trie = singleExportTrie("_resolved", spec);
        auto found = ExportsTrie::lookUp(trie.span(), "_resolved");
        TEST_ASSERT(found, "a stub-and-resolver export is found");
        if (found) {
            TEST_ASSERT(found->kind == Kind::Regular, "a stub-and-resolver export is Regular");
            TEST_ASSERT_HEX_EQ(found->value, 0x1000, "a stub-and-resolver export yields the stub offset");
        }
    }
    {
        // The highest defined flag bit. It carries no payload of its own, so a
        // terminal that sets it still decodes; nothing in dyld, ld or cctools reads
        // it, which is why it must not be mistaken for an unknown bit.
        TerminalSpec spec;
        spec.flags = EXPORT_SYMBOL_FLAGS_STATIC_RESOLVER | EXPORT_SYMBOL_FLAGS_KIND_REGULAR;
        spec.values.append(0x50);
        Vector<uint8_t> trie = singleExportTrie("_staticResolver", spec);
        auto found = ExportsTrie::lookUp(trie.span(), "_staticResolver");
        TEST_ASSERT(found, "an export with the highest defined flag bit is found");
        if (found) {
            TEST_ASSERT(found->kind == Kind::Regular, "a static-resolver export is Regular");
            TEST_ASSERT_HEX_EQ(found->value, 0x50, "a static-resolver export yields its offset");
        }
    }
    {
        // A terminal may declare more room than its flags and offset need. The spare
        // room is not part of the offset, and the export still resolves.
        TrieBytes builder;
        builder.uleb128(4); // Two bytes more than the payload below uses.
        builder.uleb128(0); // Flags: an ordinary export.
        builder.uleb128(0x7f); // Offset.
        builder.byte(0); // Spare terminal byte.
        builder.byte(0); // Spare terminal byte.
        builder.byte(0); // No children.
        Vector<uint8_t> trie = builder.take();
        auto found = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(found, "a terminal with room to spare still resolves");
        if (found)
            TEST_ASSERT_HEX_EQ(found->value, 0x7f, "spare terminal room is not read as the offset");
    }
    {
        // A look up of "" reaches the root's own terminal without walking an edge.
        Vector<uint8_t> trie = terminalOnlyTrie(regularExport(0x99));
        auto found = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(found, "a terminal at the root is found");
        if (found)
            TEST_ASSERT_HEX_EQ(found->value, 0x99, "a terminal at the root yields its offset");
    }
    {
        // Real tries spread a name over several edges, so the walk has to cross
        // more than one node to reach the terminal.
        std::array<std::string_view, 3> edges { "_f", "oo", "bar" };
        Vector<uint8_t> trie = chainedExportTrie(std::span<const std::string_view>(edges), regularExport(0x77));
        auto found = ExportsTrie::lookUp(trie.span(), "_foobar");
        TEST_ASSERT(found, "a name spread over several edges is found");
        if (found)
            TEST_ASSERT_HEX_EQ(found->value, 0x77, "a multi-edge name yields its offset");

        auto prefix = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!prefix && prefix.error() == Failure::Absent,
            "a prefix of a multi-edge name is absent");
        auto extension = ExportsTrie::lookUp(trie.span(), "_foobarbaz");
        TEST_ASSERT(!extension && extension.error() == Failure::Absent,
            "an extension of a multi-edge name is absent");
    }
    {
        // "_foo" and "_foobar" both exported: the shorter one lives on a node that
        // also has children, so a terminal must not end the walk when name is left.
        Vector<uint8_t> trie = prefixAndChildTrie("_foo", regularExport(0x10), "bar", regularExport(0x20));
        auto shorter = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(shorter, "the shorter of two nested names is found");
        if (shorter)
            TEST_ASSERT_HEX_EQ(shorter->value, 0x10, "the shorter name yields its own offset");
        auto longer = ExportsTrie::lookUp(trie.span(), "_foobar");
        TEST_ASSERT(longer, "the longer of two nested names is found");
        if (longer)
            TEST_ASSERT_HEX_EQ(longer->value, 0x20, "the longer name yields its own offset");
        TEST_ASSERT(!ExportsTrie::lookUp(trie.span(), "_foobaz"), "a name that diverges is not found");
    }
    {
        // A node's children count is a byte rather than a ULEB128. Decoded as one, a
        // count of 255 carries a continuation bit, so it would swallow the first
        // character of the edge behind it: the count comes out far too large and that
        // edge is read from the wrong byte. 255 edges is the widest a node can be, and
        // reaching the last of them needs the walk to scan past every edge ahead of it.
        Vector<uint8_t> trie = fanOutTrie(maximumEdgeCount);
        for (unsigned index : { 0u, maximumEdgeCount / 2, maximumEdgeCount - 1 }) {
            auto found = ExportsTrie::lookUp(trie.span(), fanOutEdgeName(index));
            TEST_ASSERT(found, "an export is found among 255 edges");
            if (found) {
                TEST_ASSERT_HEX_EQ(found->value, fanOutExportOffset(index),
                    "each of 255 edges leads to its own export");
            }
        }

        // Nothing matches, so the walk has to scan all 255 edges and end.
        auto absent = ExportsTrie::lookUp(trie.span(), "zz");
        TEST_ASSERT(!absent && absent.error() == Failure::Absent,
            "a name matching none of 255 edges is Absent");
    }
}

static void testClassifiedFailures()
{
    {
        TerminalSpec spec;
        spec.flags = EXPORT_SYMBOL_FLAGS_REEXPORT | EXPORT_SYMBOL_FLAGS_KIND_REGULAR;
        spec.values.append(1); // Library ordinal.
        spec.trailingString = "_other";
        spec.hasTrailingString = true;
        Vector<uint8_t> trie = singleExportTrie("_reexported", spec);
        auto result = ExportsTrie::lookUp(trie.span(), "_reexported");
        TEST_ASSERT(!result && result.error() == Failure::ReExport,
            "a re-exported name reports ReExport rather than being absent");
    }
    {
        TerminalSpec spec;
        spec.flags = EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL;
        spec.values.append(0x30);
        Vector<uint8_t> trie = singleExportTrie("_threadLocal", spec);
        auto result = ExportsTrie::lookUp(trie.span(), "_threadLocal");
        TEST_ASSERT(!result && result.error() == Failure::UnsupportedKind,
            "a thread-local reports UnsupportedKind: its address differs per thread");
    }
    {
        // The one value the kind mask can hold that Mach-O does not define. A kind
        // this code does not know cannot be read as an address.
        TerminalSpec spec;
        spec.flags = 0x03;
        spec.values.append(0x30);
        Vector<uint8_t> trie = singleExportTrie("_unknownKind", spec);
        auto result = ExportsTrie::lookUp(trie.span(), "_unknownKind");
        TEST_ASSERT(!result && result.error() == Failure::UnsupportedKind,
            "an unrecognized kind reports UnsupportedKind");
    }
    {
        Vector<uint8_t> trie = singleExportTrie("_foo", regularExport(0x1234));
        auto missing = ExportsTrie::lookUp(trie.span(), "_bar");
        TEST_ASSERT(!missing && missing.error() == Failure::Absent,
            "a name with no matching edge is Absent");
        auto shortName = ExportsTrie::lookUp(trie.span(), "_fo");
        TEST_ASSERT(!shortName && shortName.error() == Failure::Absent,
            "a name shorter than the edge is Absent");
        auto emptyName = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!emptyName && emptyName.error() == Failure::Absent,
            "the empty name is Absent when the root exports nothing");
    }
    {
        // A node with neither a terminal nor children ends the walk without an answer.
        TrieBytes builder;
        builder.uleb128(0);
        builder.byte(0);
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Absent, "a childless root is Absent");
    }
}

static void testMalformedTries()
{
    {
        Vector<uint8_t> empty;
        auto result = ExportsTrie::lookUp(empty.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed, "an empty trie is malformed");
    }
    {
        Vector<uint8_t> trie { 0x80 }; // A terminal length that never ends.
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a truncated terminal length is malformed");
    }
    {
        // A terminal claiming more bytes than the trie has left. Rejecting this is
        // what keeps the children position inside the buffer.
        Vector<uint8_t> trie { 0x7f };
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a terminal longer than the trie is malformed");
    }
    {
        // A terminal length so large that adding it to the position would wrap.
        TrieBytes builder;
        builder.uleb128(std::numeric_limits<uint64_t>::max());
        builder.byte(0);
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a terminal length that would wrap the position is malformed");
    }
    {
        Vector<uint8_t> trie { 0x01, 0x80 }; // Terminal of one byte, holding half a ULEB128.
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed, "truncated flags are malformed");
    }
    {
        Vector<uint8_t> trie { 0x01, 0x00 }; // Flags say an ordinary export, but no offset follows.
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "an export with no offset is malformed");
    }
    {
        // Flags promise a stub offset that the trie does not hold.
        TrieBytes builder;
        TrieBytes payload;
        payload.uleb128(EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER | EXPORT_SYMBOL_FLAGS_KIND_REGULAR);
        Vector<uint8_t> payloadBytes = payload.take();
        builder.uleb128(payloadBytes.size());
        builder.bytes(payloadBytes.span());
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a stub-and-resolver export with no offsets is malformed");
    }
    {
        // Only six flag bits are defined. An unknown one may carry a ULEB128 ahead of
        // the address, so the offset that follows it cannot be trusted to be one.
        TerminalSpec spec;
        spec.flags = 0x40;
        spec.values.append(0x42);
        Vector<uint8_t> trie = singleExportTrie("_unknownFlag", spec);
        auto result = ExportsTrie::lookUp(trie.span(), "_unknownFlag");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a terminal with an unknown flag bit is malformed");
    }
    {
        // A terminal that declares only its flags holds no offset. Reading one anyway
        // would take the children count that follows it as the symbol's address.
        TrieBytes builder;
        builder.uleb128(1); // Terminal length: room for the flags alone.
        builder.uleb128(0); // Flags: an ordinary export, which needs an offset...
        builder.byte(2); // ...but this is the children count, not one.
        builder.cString("a");
        builder.uleb128(0);
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a terminal that ends before its offset is malformed");
    }
    {
        // The offset's encoding runs off the end of the terminal, so completing it
        // would take a byte belonging to the children.
        TrieBytes builder;
        builder.uleb128(2); // Terminal length: the flags and one offset byte.
        builder.uleb128(0); // Flags.
        builder.byte(0x80); // First byte of a two-byte offset...
        builder.byte(0x01); // ...whose second byte lies outside the terminal.
        builder.byte(0); // No children.
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "an offset whose encoding leaves the terminal is malformed");
    }
    {
        // The children count sits past the end of the trie.
        Vector<uint8_t> trie { 0x00 };
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a missing children count is malformed");
    }
    {
        TrieBytes builder;
        builder.uleb128(0);
        builder.byte(1);
        builder.string("_foo"); // No terminator.
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "an unterminated edge is malformed");
    }
    {
        // An empty edge would match anything and consume none of the name, which is
        // what would let a walk run forever.
        TrieBytes builder;
        builder.uleb128(0);
        builder.byte(1);
        builder.cString("");
        builder.uleb128(0);
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed, "an empty edge is malformed");
    }
    {
        TrieBytes builder;
        builder.uleb128(0);
        builder.byte(1);
        builder.cString("_foo");
        builder.byte(0x80); // A child offset that never ends.
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a truncated child offset is malformed");
    }
    {
        // An edge leading outside the trie.
        TrieBytes builder;
        builder.uleb128(0);
        builder.byte(1);
        builder.cString("_foo");
        builder.uleb128(1000);
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result && result.error() == Failure::Malformed,
            "a child offset past the end of the trie is malformed");
    }
    {
        // An edge leading into the middle of the node it came from. Whatever that
        // decodes to, it must be an answer rather than a crash or a hang.
        TrieBytes builder;
        builder.uleb128(0);
        builder.byte(1);
        builder.cString("_foo");
        builder.uleb128(2);
        Vector<uint8_t> trie = builder.take();
        auto result = ExportsTrie::lookUp(trie.span(), "_foo");
        TEST_ASSERT(!result, "a child offset into the middle of a node yields no export");
    }
    {
        // A trie truncated part way through a node it claims to hold. The last byte
        // is the terminal node's children count, which a look up that ends at that
        // terminal never reads, so removing only that byte still resolves; every
        // shorter prefix cuts into the terminal itself and must not.
        Vector<uint8_t> trie = singleExportTrie("_foo", regularExport(0x1234));
        for (size_t length = 1; length + 1 < trie.size(); ++length) {
            auto result = ExportsTrie::lookUp(trie.span().first(length), "_foo");
            TEST_ASSERT(!result, "a truncated trie yields no export");
        }
    }
}

static void testWalkIsBounded()
{
    // A node whose only edge leads back to itself. The walk may only follow an edge
    // by consuming at least one character of the name, so it has to end even though
    // the trie describes a cycle. Without that property this test would not return.
    TrieBytes builder;
    builder.uleb128(0);
    builder.byte(1);
    builder.cString("a");
    builder.uleb128(0); // Back to the root.
    Vector<uint8_t> trie = builder.take();

    std::string name(20000, 'a');
    auto result = ExportsTrie::lookUp(trie.span(), name);
    TEST_ASSERT(!result, "a cyclic trie yields no export");

    // The same cycle reached with a name it cannot consume.
    auto other = ExportsTrie::lookUp(trie.span(), "b");
    TEST_ASSERT(!other && other.error() == Failure::Absent, "a cycle whose edge does not match is Absent");
}

void testExportsTrie()
{
    SuiteTracer tracer("ExportsTrie");
    if (!tracer.shouldRun())
        return;

    testFoundExports();
    testClassifiedFailures();
    testMalformedTries();
    testWalkIsBounded();
}

// A small deterministic generator, so that a failure can be reproduced from the
// seed the run reports.
class Random {
public:
    explicit Random(uint64_t seed)
        : m_state(seed ? seed : 0x9e3779b97f4a7c15ull)
    {
    }

    uint64_t next()
    {
        m_state ^= m_state >> 12;
        m_state ^= m_state << 25;
        m_state ^= m_state >> 27;
        return m_state * 0x2545f4914f6cdd1dull;
    }

    uint32_t below(uint32_t bound) { return bound ? static_cast<uint32_t>(next() % bound) : 0; }

private:
    uint64_t m_state;
};

static Atomic<uint64_t> fuzzIteration;
static Atomic<bool> fuzzFinished;

static void* fuzzWatchdog(void*)
{
    uint64_t lastSeen = 0;
    unsigned stalledPolls = 0;
    static constexpr unsigned pollIntervalUsec = 250 * 1000;
    static constexpr unsigned stallLimitPolls = 40; // Ten seconds.

    while (!fuzzFinished.load()) {
        usleep(pollIntervalUsec);
        uint64_t current = fuzzIteration.load();
        if (current != lastSeen) {
            lastSeen = current;
            stalledPolls = 0;
            continue;
        }
        if (++stalledPolls < stallLimitPolls)
            continue;
        // The decoder promises to bound its work on any input. A stall means it
        // does not, so crash here rather than let the run hang: a report with a
        // stack in the decoder says far more than a timeout does.
        dataLogLn("FAIL: exports trie look up did not finish on fuzz iteration ", lastSeen);
        CRASH();
    }
    return nullptr;
}

void fuzzExportsTrie(uint64_t seed, unsigned iterations)
{
    SuiteTracer tracer("ExportsTrie fuzz");
    if (!tracer.shouldRun())
        return;
    dataLogLn("    seed ", RawHex(seed), ", ", iterations, " iterations");

    Random random(seed);
    fuzzIteration.store(0);
    fuzzFinished.store(false);

    pthread_t watchdog { };
    bool watching = !pthread_create(&watchdog, nullptr, fuzzWatchdog, nullptr);
    TEST_ASSERT(watching, "the fuzz watchdog started");

    Vector<uint8_t> valid = singleExportTrie("_foo", regularExport(0x1234));
    std::array<std::string_view, 6> names { "_foo", "_foobar", "_f", "", "_bar", "_fop" };

    for (unsigned iteration = 0; iteration < iterations; ++iteration) {
        fuzzIteration.store(iteration + 1);

        Vector<uint8_t> trie;
        if (random.below(4)) {
            // Mostly near-valid tries: those reach further into the decoder than
            // noise does, because their early fields still make sense.
            trie = valid;
            unsigned mutations = 1 + random.below(6);
            for (unsigned mutation = 0; mutation < mutations; ++mutation)
                trie[random.below(static_cast<uint32_t>(trie.size()))] = static_cast<uint8_t>(random.next());
        } else {
            unsigned length = random.below(64);
            for (unsigned index = 0; index < length; ++index)
                trie.append(static_cast<uint8_t>(random.next()));
        }

        std::string generatedName;
        std::string_view name;
        if (random.below(4))
            name = names[random.below(names.size())];
        else {
            unsigned length = random.below(12);
            for (unsigned index = 0; index < length; ++index)
                generatedName += static_cast<char>('_' + random.below(48));
            name = generatedName;
        }

        auto result = ExportsTrie::lookUp(trie.span(), name);
        // Noise is allowed to decode as an export. What is not allowed is an
        // export of a kind the caller cannot act on.
        if (result) {
            TEST_ASSERT(result->kind == Kind::Regular || result->kind == Kind::Absolute,
                "a decoded export has a kind the caller understands");
        }
    }

    fuzzFinished.store(true);
    if (watching)
        pthread_join(watchdog, nullptr);
}

} // namespace JSCToolsTest

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
