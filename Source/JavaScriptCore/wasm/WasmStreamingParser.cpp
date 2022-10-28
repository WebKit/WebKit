/*
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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
#include "WasmStreamingParser.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmModuleInformation.h"
#include "WasmOps.h"
#include "WasmParser.h"
#include "WasmSectionParser.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/FileSystem.h>
#include <wtf/UnalignedAccess.h>

namespace JSC { namespace Wasm {

namespace WasmStreamingParserInternal {
static constexpr bool verbose = false;
}

#define WASM_STREAMING_PARSER_FAIL_IF_HELPER_FAILS(helper) \
    do { \
        auto helperResult = helper; \
        if (UNLIKELY(!helperResult)) { \
            m_errorMessage = helperResult.error(); \
            return State::FatalError; \
        } \
    } while (0)

ALWAYS_INLINE std::optional<uint8_t> parseUInt7(const uint8_t* data, size_t& offset, size_t size)
{
    if (offset >= size)
        return false;
    uint8_t result = data[offset++];
    if (result < 0x80)
        return result;
    return std::nullopt;
}

template <typename ...Args>
NEVER_INLINE auto WARN_UNUSED_RETURN StreamingParser::fail(Args... args) -> State
{
    using namespace FailureHelper; // See ADL comment in namespace above.
    m_errorMessage = makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_offset), ": "_s, makeString(args)...);
    dataLogLnIf(WasmStreamingParserInternal::verbose, m_errorMessage);
    return State::FatalError;
}

#if ENABLE(WEBASSEMBLY)
static void dumpWasmSource(const Vector<uint8_t>& source)
{
    static int count = 0;
    const char* file = Options::dumpWasmSourceFileName();
    if (!file)
        return;
    auto fileHandle = FileSystem::openFile(WTF::makeString(file, (count++), ".wasm"_s), 
        FileSystem::FileOpenMode::Write,
        FileSystem::FileAccessPermission::All,
        /* failIfFileExists = */ true);
    if (fileHandle == FileSystem::invalidPlatformFileHandle) {
        dataLogLn("Error dumping wasm");
        return;
    }
    dataLogLn("Dumping ", source.size(), " wasm source bytes to ", WTF::makeString(file, (count - 1), ".wasm"_s));
    FileSystem::writeToFile(fileHandle, source.data(), source.size());
    FileSystem::closeFile(fileHandle);
}
#endif

StreamingParser::StreamingParser(ModuleInformation& info, StreamingParserClient& client)
    : m_info(info)
    , m_client(client)
{
    dataLogLnIf(WasmStreamingParserInternal::verbose, "starting validation");

#if ASSERT_ENABLED
    dataLogLnIf(Options::dumpWasmSourceFileName(), "Wasm streaming parser created, capturing source.");
#else
    dataLogLnIf(Options::dumpWasmSourceFileName(), "Wasm streaming parser created, but we can only dump source in debug builds.");
#endif
}

auto StreamingParser::parseModuleHeader(Vector<uint8_t>&& data) -> State
{
    ASSERT(data.size() == moduleHeaderSize);
    dataLogLnIf(WasmStreamingParserInternal::verbose, "header validation");
    WASM_PARSER_FAIL_IF(data[0] != '\0' || data[1] != 'a' || data[2] != 's' || data[3] != 'm', "module doesn't start with '\\0asm'");
    uint32_t versionNumber = WTF::unalignedLoad<uint32_t>(data.data() + 4);
    WASM_PARSER_FAIL_IF(versionNumber != expectedVersionNumber, "unexpected version number ", versionNumber, " expected ", expectedVersionNumber);
    return State::SectionID;
}

auto StreamingParser::parseSectionID(Vector<uint8_t>&& data) -> State
{
    ASSERT(data.size() == sectionIDSize);
    size_t offset = 0;
    auto result = parseUInt7(data.data(), offset, data.size());
    WASM_PARSER_FAIL_IF(!result, "can't get section byte");

    Section section = Section::Custom;
    WASM_PARSER_FAIL_IF(!decodeSection(*result, section), "invalid section");
    ASSERT(section != Section::Begin);
    WASM_PARSER_FAIL_IF(!validateOrder(m_previousKnownSection, section), "invalid section order, ", m_previousKnownSection, " followed by ", section);
    m_section = section;
    if (isKnownSection(section))
        m_previousKnownSection = section;
    return State::SectionSize;
}

auto StreamingParser::parseSectionSize(uint32_t sectionLength) -> State
{
    m_sectionLength = sectionLength;
    if (m_section == Section::Code)
        return State::CodeSectionSize;
    return State::SectionPayload;
}

auto StreamingParser::parseCodeSectionSize(uint32_t functionCount) -> State
{
    m_info->codeSectionSize = m_sectionLength;
    m_functionCount = functionCount;
    m_functionIndex = 0;
    m_codeOffset = m_offset;

    WASM_PARSER_FAIL_IF(functionCount == std::numeric_limits<uint32_t>::max(), "Code section's count is too big ", functionCount);
    WASM_PARSER_FAIL_IF(functionCount != m_info->functions.size(), "Code section count ", functionCount, " exceeds the declared number of functions ", m_info->functions.size());

    if (m_functionIndex == m_functionCount) {
        WASM_PARSER_FAIL_IF((m_codeOffset + m_sectionLength) != m_nextOffset, "parsing ended before the end of ", m_section, " section");
        if (!m_client.didReceiveSectionData(m_section))
            return State::FatalError;
        return State::SectionID;
    }
    return State::FunctionSize;
}

auto StreamingParser::parseFunctionSize(uint32_t functionSize) -> State
{
    m_functionSize = functionSize;
    WASM_PARSER_FAIL_IF(functionSize > maxFunctionSize, "Code function's size ", functionSize, " is too big");
    return State::FunctionPayload;
}

auto StreamingParser::parseFunctionPayload(Vector<uint8_t>&& data) -> State
{
    auto& function = m_info->functions[m_functionIndex];
    function.start = m_offset;
    function.end = m_offset + m_functionSize;
    function.data = WTFMove(data);
    dataLogLnIf(WasmStreamingParserInternal::verbose, "Processing function starting at: ", function.start, " and ending at: ", function.end);
    if (!m_client.didReceiveFunctionData(m_functionIndex, function))
        return State::FatalError;
    ++m_functionIndex;

    if (m_functionIndex == m_functionCount) {
        WASM_PARSER_FAIL_IF((m_codeOffset + m_sectionLength) != (m_offset + m_functionSize), "parsing ended before the end of ", m_section, " section");
        if (!m_client.didReceiveSectionData(m_section))
            return State::FatalError;
        return State::SectionID;
    }
    return State::FunctionSize;
}

auto StreamingParser::parseSectionPayload(Vector<uint8_t>&& data) -> State
{
    SectionParser parser(data.data(), data.size(), m_offset, m_info.get());
    switch (m_section) {
#define WASM_SECTION_PARSE(NAME, ID, ORDERING, DESCRIPTION) \
    case Section::NAME: { \
        WASM_STREAMING_PARSER_FAIL_IF_HELPER_FAILS(parser.parse ## NAME()); \
        break; \
    }
    FOR_EACH_KNOWN_WASM_SECTION(WASM_SECTION_PARSE)
#undef WASM_SECTION_PARSE

    case Section::Custom: {
        WASM_STREAMING_PARSER_FAIL_IF_HELPER_FAILS(parser.parseCustom());
        break;
    }

    case Section::Begin: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    }

    WASM_PARSER_FAIL_IF(parser.length() != parser.offset(), "parsing ended before the end of ", m_section, " section");

    if (!m_client.didReceiveSectionData(m_section))
        return State::FatalError;
    return State::SectionID;
}

auto StreamingParser::consume(const uint8_t* bytes, size_t bytesSize, size_t& offsetInBytes, size_t requiredSize) -> std::optional<Vector<uint8_t>>
{
    if (m_remaining.size() == requiredSize) {
        Vector<uint8_t> result = WTFMove(m_remaining);
        m_nextOffset += requiredSize;
        return result;
    }

    if (m_remaining.size() > requiredSize) {
        Vector<uint8_t> result { m_remaining.data(), requiredSize };
        m_remaining.remove(0, requiredSize);
        m_nextOffset += requiredSize;
        return result;
    }

    ASSERT(m_remaining.size() < requiredSize);
    size_t bytesRemainingSize = bytesSize - offsetInBytes;
    size_t totalDataSize = m_remaining.size() + bytesRemainingSize;
    if (totalDataSize < requiredSize) {
        m_remaining.append(bytes + offsetInBytes, bytesRemainingSize);
        offsetInBytes = bytesSize;
        return std::nullopt;
    }

    size_t usedSize = requiredSize - m_remaining.size();
    m_remaining.append(bytes + offsetInBytes, usedSize);
    offsetInBytes += usedSize;
    Vector<uint8_t> result = WTFMove(m_remaining);
    m_nextOffset += requiredSize;
    return result;
}

auto StreamingParser::consumeVarUInt32(const uint8_t* bytes, size_t bytesSize, size_t& offsetInBytes, IsEndOfStream isEndOfStream) -> Expected<uint32_t, State>
{
    constexpr size_t maxSize = WTF::LEBDecoder::maxByteLength<uint32_t>();
    size_t bytesRemainingSize = bytesSize - offsetInBytes;
    size_t totalDataSize = m_remaining.size() + bytesRemainingSize;
    if (m_remaining.size() >= maxSize) {
        // Do nothing.
    } else if (totalDataSize >= maxSize) {
        size_t usedSize = maxSize - m_remaining.size();
        m_remaining.append(bytes + offsetInBytes, usedSize);
        offsetInBytes += usedSize;
    } else {
        m_remaining.append(bytes + offsetInBytes, bytesRemainingSize);
        offsetInBytes += bytesRemainingSize;
        // If the given bytes are the end of the stream, we try to parse VarUInt32
        // with the current remaining data since VarUInt32 may not require `maxSize`.
        if (isEndOfStream == IsEndOfStream::No)
            return makeUnexpected(m_state);
    }

    size_t offset = 0;
    uint32_t result = 0;
    if (!WTF::LEBDecoder::decodeUInt32(m_remaining.data(), m_remaining.size(), offset, result))
        return makeUnexpected(State::FatalError);
    size_t consumedSize = offset;
    m_remaining.remove(0, consumedSize);
    m_nextOffset += consumedSize;
    return result;
}

auto StreamingParser::addBytes(const uint8_t* bytes, size_t bytesSize, IsEndOfStream isEndOfStream) -> State
{
#if ASSERT_ENABLED
    if (Options::dumpWasmSourceFileName()) {
        for (unsigned i = 0; i < bytesSize; ++i)
            m_buffer.append(bytes[i]);

        if (isEndOfStream == IsEndOfStream::Yes) {
            dataLogLn("Streaming parser reached end of stream.");
            dumpWasmSource(m_buffer);
        }
    }
#else
    (void) dumpWasmSource;
#endif
    if (m_state == State::FatalError)
        return m_state;

    m_totalSize += bytesSize;
    if (UNLIKELY(m_totalSize.hasOverflowed() || m_totalSize > maxModuleSize)) {
        m_state = fail("module size is too large, maximum ", maxModuleSize);
        return m_state;
    }

    if (UNLIKELY(Options::useEagerWebAssemblyModuleHashing()))
        m_hasher.addBytes(bytes, bytesSize);

    size_t offsetInBytes = 0;
    while (true) {
        ASSERT(offsetInBytes <= bytesSize);
        switch (m_state) {
        case State::ModuleHeader: {
            auto result = consume(bytes, bytesSize, offsetInBytes, moduleHeaderSize);
            if (!result)
                return m_state;
            m_state = parseModuleHeader(WTFMove(*result));
            break;
        }

        case State::SectionID: {
            auto result = consume(bytes, bytesSize, offsetInBytes, sectionIDSize);
            if (!result)
                return m_state;
            m_state = parseSectionID(WTFMove(*result));
            break;
        }

        case State::SectionSize: {
            auto result = consumeVarUInt32(bytes, bytesSize, offsetInBytes, isEndOfStream);
            if (!result) {
                if (result.error() == State::FatalError)
                    m_state = failOnState(m_state);
                else
                    m_state = result.error();
                return m_state;
            }
            m_state = parseSectionSize(*result);
            break;
        }

        case State::SectionPayload: {
            auto result = consume(bytes, bytesSize, offsetInBytes, m_sectionLength);
            if (!result)
                return m_state;
            m_state = parseSectionPayload(WTFMove(*result));
            break;
        }

        case State::CodeSectionSize: {
            auto result = consumeVarUInt32(bytes, bytesSize, offsetInBytes, isEndOfStream);
            if (!result) {
                if (result.error() == State::FatalError)
                    m_state = failOnState(m_state);
                else
                    m_state = result.error();
                return m_state;
            }
            m_state = parseCodeSectionSize(*result);
            break;
        }

        case State::FunctionSize: {
            auto result = consumeVarUInt32(bytes, bytesSize, offsetInBytes, isEndOfStream);
            if (!result) {
                if (result.error() == State::FatalError)
                    m_state = failOnState(m_state);
                else
                    m_state = result.error();
                return m_state;
            }
            m_state = parseFunctionSize(*result);
            break;
        }

        case State::FunctionPayload: {
            auto result = consume(bytes, bytesSize, offsetInBytes, m_functionSize);
            if (!result)
                return m_state;
            m_state = parseFunctionPayload(WTFMove(*result));
            break;
        }

        case State::Finished:
        case State::FatalError:
            return m_state;
        }

        m_offset = m_nextOffset;
    }
}

auto StreamingParser::failOnState(State) -> State
{
    switch (m_state) {
    case State::ModuleHeader:
        return fail("expected a module of at least ", moduleHeaderSize, " bytes");
    case State::SectionID:
        return fail("can't get section byte");
    case State::SectionSize:
        return fail("can't get ", m_section, " section's length");
    case State::SectionPayload:
        return fail(m_section, " section of size ", m_sectionLength, " would overflow Module's size");
    case State::CodeSectionSize:
        return fail("can't get Code section's count");
    case State::FunctionSize:
        return fail("can't get ", m_functionIndex, "th Code function's size");
    case State::FunctionPayload:
        return fail("Code function's size ", m_functionSize, " exceeds the module's remaining size");
    case State::Finished:
    case State::FatalError:
        return m_state;
    }
    return m_state;
}

auto StreamingParser::finalize() -> State
{
    addBytes(nullptr, 0, IsEndOfStream::Yes);
    switch (m_state) {
    case State::ModuleHeader:
    case State::SectionSize:
    case State::SectionPayload:
    case State::CodeSectionSize:
    case State::FunctionSize:
    case State::FunctionPayload:
        m_state = failOnState(m_state);
        break;

    case State::Finished:
    case State::FatalError:
        break;

    case State::SectionID:
        if (m_functionIndex != m_info->functions.size()) {
            m_state = fail("Number of functions parsed (", m_functionCount, ") does not match the number of declared functions (", m_info->functions.size(), ")");
            break;
        }

        if (m_info->numberOfDataSegments) {
            if (UNLIKELY(m_info->data.size() != m_info->numberOfDataSegments.value())) {
                m_state = fail("Data section's count ", m_info->data.size(), " is different from Data Count section's count ", m_info->numberOfDataSegments.value());
                break;
            }
        }

        if (m_remaining.isEmpty()) {
            if (UNLIKELY(Options::useEagerWebAssemblyModuleHashing()))
                m_info->nameSection->setHash(m_hasher.computeHexDigest());

            m_state = State::Finished;
            m_client.didFinishParsing();
        } else
            m_state = failOnState(State::SectionID);
        break;
    }
    return m_state;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
