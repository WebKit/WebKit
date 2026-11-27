/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceProvider.h"

#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/ProcessID.h>
#include <wtf/text/MakeString.h>

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringSourceProvider);

SourceProvider::SourceProvider(const SourceOrigin& sourceOrigin, String&& sourceURL, String&& preRedirectURL, SourceTaintedness taintedness, const TextPosition& startPosition, SourceProviderSourceType sourceType)
    : m_sourceType(sourceType)
    , m_sourceOrigin(sourceOrigin)
    , m_sourceURL(WTF::move(sourceURL))
    , m_preRedirectURL(WTF::move(preRedirectURL))
    , m_startPosition(startPosition)
    , m_taintedness(taintedness)
{
}

SourceProvider::~SourceProvider() = default;

void SourceProvider::lockUnderlyingBuffer()
{
    if (!m_lockingCount++)
        lockUnderlyingBufferImpl();
}

void SourceProvider::unlockUnderlyingBuffer()
{
    if (!--m_lockingCount)
        unlockUnderlyingBufferImpl();
}

CodeBlockHash SourceProvider::codeBlockHashConcurrently(int startOffset, int endOffset, CodeSpecializationKind kind) const
{
    CodeBlockHash result;
    withSourceConcurrently([&](StringView entireSourceCode) {
        result = CodeBlockHash { entireSourceCode.substring(startOffset, endOffset - startOffset), entireSourceCode, kind };
    });
    return result;
}

void SourceProvider::withSourceConcurrently(const ScopedLambda<void(StringView)>& function) const
{
    function(source());
}

void SourceProvider::lockUnderlyingBufferImpl() { }

void SourceProvider::unlockUnderlyingBufferImpl() { }

void SourceProvider::getID()
{
    if (!m_id) {
        static std::atomic<SourceID> nextProviderID = nullID;
        m_id = ++nextProviderID;
        RELEASE_ASSERT(m_id);
    }
}

const String& SourceProvider::sourceURLStripped()
{
    if (m_sourceURL.isNull()) [[unlikely]]
        return m_sourceURLStripped;
    if (!m_sourceURLStripped.isNull()) [[likely]]
        return m_sourceURLStripped;
    m_sourceURLStripped = URL(m_sourceURL).strippedForUseAsReport();
    return m_sourceURLStripped;
}

UTF8CString SourceProvider::sourceCodeDumpFilePath(const UTF8CString& dumpDirectory)
{
    if (m_sourceCodeDumped.load(std::memory_order_acquire)) {
        Locker locker { m_sourceCodeDumpLock };
        return m_sourceCodeDumpFilePath;
    }

    Locker locker { m_sourceCodeDumpLock };
    if (m_sourceCodeDumped.load(std::memory_order_relaxed))
        return m_sourceCodeDumpFilePath;

    auto tryExtractLocalPath = [](const String& urlString) -> String {
        if (urlString.isNull())
            return { };
        if (urlString.startsWith('/'))
            return urlString;
        if (urlString.startsWith("file://"_s))
            return URL(urlString).fileSystemPath();
        return { };
    };

    String localPath = tryExtractLocalPath(sourceURL());

    if (!localPath.isNull())
        m_sourceCodeDumpFilePath = FileSystem::fileSystemRepresentation(localPath);
    else {
        auto baseName = makeString("source-"_s, asID(), '-', WTF::getCurrentProcessID());
        String filePath;
        FileSystem::FileHandle handle;
        if (dumpDirectory.isNull()) {
            auto result = FileSystem::openTemporaryFile(baseName, ".js"_s);
            filePath = result.first;
            handle = WTF::move(result.second);
        } else {
            filePath = makeString(dumpDirectory, FileSystem::pathSeparator, baseName, ".js"_s);
            handle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Truncate);
        }
        if (handle) {
            auto sourceText = makeUTF8CString(source());
            handle.write(WTF::asByteSpan(sourceText.span()));
            handle.flush();
            m_sourceCodeDumpFilePath = FileSystem::fileSystemRepresentation(filePath);
        }
    }

    m_sourceCodeDumped.store(true, std::memory_order_release);
    return m_sourceCodeDumpFilePath;
}

#if ENABLE(WEBASSEMBLY)
BaseWebAssemblySourceProvider::BaseWebAssemblySourceProvider(const SourceOrigin& sourceOrigin, String&& sourceURL)
    : SourceProvider(sourceOrigin, WTF::move(sourceURL), String(), SourceTaintedOrigin::Untainted, TextPosition(), SourceProviderSourceType::WebAssembly)
{
}
#endif

#if CPU(ARM64)
template<typename CharType>
static constexpr size_t lineStartBlockSize = 64 / sizeof(CharType);

// Returns one bit per character of the block, set where a line ends: at every line terminator
// except a CR that begins a CRLF pair. `block` carries one character past the block so that a pair
// split across two blocks is still seen.
template<typename CharType>
ALWAYS_INLINE static uint64_t lineEndBitsInBlock(std::span<const CharType, lineStartBlockSize<CharType> + 1> block)
{
    constexpr uint8_t weightValues[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
    const auto weights = simde_vld1q_u8(weightValues);
    if constexpr (std::is_same_v<CharType, Latin1Character>) {
        auto weighted = [&](std::span<const CharType, 17> input) ALWAYS_INLINE_LAMBDA {
            auto vector = simde_vld1q_u8(std::bit_cast<const uint8_t*>(input.data()));
            auto nextIsLineFeed = simde_vceqq_u8(simde_vld1q_u8(std::bit_cast<const uint8_t*>(input.template subspan<1>().data())), simde_vdupq_n_u8('\n'));
            auto lineFeed = simde_vceqq_u8(vector, simde_vdupq_n_u8('\n'));
            auto loneCarriageReturn = simde_vbicq_u8(simde_vceqq_u8(vector, simde_vdupq_n_u8('\r')), nextIsLineFeed);
            return simde_vandq_u8(simde_vorrq_u8(lineFeed, loneCarriageReturn), weights);
        };
        auto packed = simde_vpaddq_u8(
            simde_vpaddq_u8(weighted(block.template subspan<0, 17>()), weighted(block.template subspan<16, 17>())),
            simde_vpaddq_u8(weighted(block.template subspan<32, 17>()), weighted(block.template subspan<48, 17>())));
        packed = simde_vpaddq_u8(packed, packed);
        return simde_vgetq_lane_u64(simde_vreinterpretq_u64_u8(packed), 0);
    } else {
        auto narrowed = [&](std::span<const CharType, 9> input) ALWAYS_INLINE_LAMBDA {
            auto vector = simde_vld1q_u16(std::bit_cast<const uint16_t*>(input.data()));
            auto nextIsLineFeed = simde_vceqq_u16(simde_vld1q_u16(std::bit_cast<const uint16_t*>(input.template subspan<1>().data())), simde_vdupq_n_u16('\n'));
            auto lineFeed = simde_vceqq_u16(vector, simde_vdupq_n_u16('\n'));
            auto loneCarriageReturn = simde_vbicq_u16(simde_vceqq_u16(vector, simde_vdupq_n_u16('\r')), nextIsLineFeed);
            auto separator = simde_vceqq_u16(simde_vandq_u16(vector, simde_vdupq_n_u16(0xFFFE)), simde_vdupq_n_u16(0x2028));
            return simde_vmovn_u16(simde_vorrq_u16(simde_vorrq_u16(lineFeed, loneCarriageReturn), separator));
        };
        auto low = simde_vandq_u8(simde_vcombine_u8(narrowed(block.template subspan<0, 9>()), narrowed(block.template subspan<8, 9>())), weights);
        auto high = simde_vandq_u8(simde_vcombine_u8(narrowed(block.template subspan<16, 9>()), narrowed(block.template subspan<24, 9>())), weights);
        auto packed = simde_vpaddq_u8(low, high);
        packed = simde_vpaddq_u8(packed, packed);
        packed = simde_vpaddq_u8(packed, packed);
        return simde_vgetq_lane_u32(simde_vreinterpretq_u32_u8(packed), 0);
    }
}

// Appends the line starts found in the 64-byte blocks at the front of the text and returns how many
// characters were scanned. The final block is left to the caller, so the character after every
// scanned block exists. Writing a block's first two line starts unconditionally leaves a branch only
// for blocks with more line ends, instead of one per line end, which typical source mispredicts.
template<typename CharType>
static size_t appendLineStartsInBlocks(std::span<const CharType> text, Vector<unsigned>& lineStarts)
{
    constexpr size_t blockSize = lineStartBlockSize<CharType>;
    std::array<unsigned, 512> buffer;
    size_t bufferSize = 0;
    size_t index = 0;
    for (; index + blockSize < text.size(); index += blockSize) {
        uint64_t mask = lineEndBitsInBlock(text.subspan(index).template first<blockSize + 1>());
        unsigned count = std::popcount(mask);
        unsigned lineStart = static_cast<unsigned>(index + 1);
        buffer[bufferSize] = lineStart + std::countr_zero(mask);
        mask &= mask - 1;
        buffer[bufferSize + 1] = lineStart + std::countr_zero(mask);
        mask &= mask - 1;
        for (unsigned i = 2; i < count; ++i) {
            buffer[bufferSize + i] = lineStart + std::countr_zero(mask);
            mask &= mask - 1;
        }
        bufferSize += count;
        if (bufferSize > buffer.size() - blockSize) [[unlikely]] {
            lineStarts.append(std::span { buffer }.first(bufferSize));
            bufferSize = 0;
        }
    }
    lineStarts.append(std::span { buffer }.first(bufferSize));
    return index;
}
#endif

template<typename CharType>
Vector<unsigned> LineStartTable::build(std::span<const CharType> text)
{
    Vector<unsigned> lineStarts;
    lineStarts.append(0);

    // A line starts after every terminator except a CR that begins a CRLF pair, whose LF starts it.
    auto addLineStartAfterTerminator = [&](size_t index) {
        if (index + 1 < text.size() && isCRLFPair(text[index], text[index + 1]))
            return;
        lineStarts.append(static_cast<unsigned>(index + 1));
    };

    using UnsignedType = SameSizeUnsignedInteger<CharType>;
    constexpr size_t stride = SIMD::stride<CharType>;
    constexpr uint64_t laneBits = (uint64_t { 1 } << SIMD::bitsPerLaneInMask<CharType>) - 1;
    size_t index = 0;
#if CPU(ARM64)
    index = appendLineStartsInBlocks(text, lineStarts);
#endif
    for (; index + stride <= text.size(); index += stride) {
        auto lanes = lineTerminatorLanes<CharType>(SIMD::load(std::bit_cast<const UnsignedType*>(text.subspan(index, stride).data())));
        for (uint64_t mask = SIMD::laneMask(lanes); mask; ) {
            unsigned bit = std::countr_zero(mask);
            addLineStartAfterTerminator(index + bit / SIMD::bitsPerLaneInMask<CharType>);
            mask &= ~(laneBits << bit);
        }
    }
    for (; index < text.size(); ++index) {
        if (isLineTerminator(text[index]))
            addLineStartAfterTerminator(index);
    }

    lineStarts.shrinkToFit();
    return lineStarts;
}

const Vector<unsigned>& LineStartTable::ensureBuilt(StringView text)
{
    if (!m_lineStarts) {
        m_lineStarts = text.is8Bit() ? build(text.span8()) : build(text.span16());
        m_builtForLength = text.length();
    }
    ASSERT(m_builtForLength == text.length());
    return *m_lineStarts;
}

static unsigned lineEndFor(StringView text, const Vector<unsigned>& lineStarts, size_t line0Based)
{
    unsigned length = text.length();
    // A non-final line's end comes from the next line's start, which is past the terminator, so the
    // terminator has to be backed over.
    unsigned lineEnd = (line0Based + 1 < lineStarts.size()) ? lineStarts[line0Based + 1] : length;
    if (lineEnd < length) {
        if (lineEnd >= 2 && isCRLFPair(text[lineEnd - 2], text[lineEnd - 1]))
            lineEnd -= 2;
        else
            lineEnd -= 1;
    }
    return lineEnd;
}

// An offset past the end of the text clamps to the last line rather than being refused.
static LineColumn zeroBasedLineColumnInTable(const Vector<unsigned>& lineStarts, unsigned offset)
{
    size_t line0Based = lineStarts.size() - 1;
    if (offset < lineStarts.last()) {
        auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset);
        ASSERT(it != lineStarts.begin());
        line0Based = static_cast<size_t>(it - lineStarts.begin()) - 1;
    }
    unsigned lineStart = lineStarts[line0Based];
    return { static_cast<unsigned>(line0Based), offset > lineStart ? offset - lineStart : 0 };
}

LineStartTable::PositionInfo LineStartTable::positionInfoForOffset(StringView text, unsigned offset)
{
    Locker locker { m_lock };
    const Vector<unsigned>& lineStarts = ensureBuilt(text);

    auto position = zeroBasedLineColumnInTable(lineStarts, offset);
    return {
        position.line,
        position.column,
        lineStarts[position.line],
        lineEndFor(text, lineStarts, position.line),
    };
}

LineColumn LineStartTable::zeroBasedLineColumnForOffset(const SourceProvider& provider, unsigned offset)
{
    Locker locker { m_lock };
    if (!m_lineStarts) {
        provider.withSourceConcurrently([&](StringView text) {
            assertIsHeld(m_lock);
            ensureBuilt(text);
        });
    }
    return zeroBasedLineColumnInTable(*m_lineStarts, offset);
}

unsigned LineStartTable::offsetForPosition(StringView text, unsigned line0Based, unsigned column0Based)
{
    Locker locker { m_lock };
    const Vector<unsigned>& lineStarts = ensureBuilt(text);

    unsigned length = text.length();
    if (line0Based >= lineStarts.size())
        return length;

    return std::min(lineStarts[line0Based] + column0Based, lineEndFor(text, lineStarts, line0Based));
}

} // namespace JSC

