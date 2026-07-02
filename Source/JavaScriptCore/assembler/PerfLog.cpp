/*
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "PerfLog.h"

#if ENABLE(ASSEMBLER)

#include "Options.h"
#include "ProfilerSupport.h"
#include "SourceProvider.h"
#include <fcntl.h>
#include <mutex>
#include <wtf/DataLog.h>
#include <wtf/FileSystem.h>
#include <wtf/PageBlock.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/TZoneMallocInlines.h>

#if OS(LINUX)
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if OS(WINDOWS)
#include <io.h>

inline static int open(const char* filename, int oflag, int pmode)
{
    return _open(filename, oflag, pmode);
}

inline static FILE* fdopen(int fd, const char* mode)
{
    return _fdopen(fd, mode);
}
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace PerfLogInternal {
static constexpr bool verbose = false;
} // namespace PerfLogInternal

namespace JITDump {
namespace Constants {

// Perf jit-dump formats are specified here.
// https://raw.githubusercontent.com/torvalds/linux/master/tools/perf/Documentation/jitdump-specification.txt

// The latest version 2, but it is too new at that time.
static constexpr uint32_t version = 1;

#if CPU(LITTLE_ENDIAN)
static constexpr uint32_t magic = 0x4a695444;
#else
static constexpr uint32_t magic = 0x4454694a;
#endif

// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
#if CPU(X86)
static constexpr uint32_t elfMachine = 0x03;
#elif CPU(X86_64)
static constexpr uint32_t elfMachine = 0x3E;
#elif CPU(ARM64)
static constexpr uint32_t elfMachine = 0xB7;
#elif CPU(ARM)
static constexpr uint32_t elfMachine = 0x28;
#elif CPU(RISCV64)
static constexpr uint32_t elfMachine = 0xF3;
#endif

} // namespace Constants

struct FileHeader {
    uint32_t magic { Constants::magic };
    uint32_t version { Constants::version };
    uint32_t totalSize { sizeof(FileHeader) };
    uint32_t elfMachine { Constants::elfMachine };
    uint32_t padding1 { 0 };
    uint32_t pid { 0 };
    uint64_t timestamp { 0 };
    uint64_t flags { 0 };
};

enum class RecordType : uint32_t {
    JITCodeLoad = 0,
    JITCodeMove = 1,
    JITCodeDebugInfo = 2,
    JITCodeClose = 3,
    JITCodeUnwindingInfo = 4,
};

struct RecordHeader {
    RecordType type { RecordType::JITCodeLoad };
    uint32_t totalSize { 0 };
    uint64_t timestamp { 0 };
};

struct CodeLoadRecord {
    RecordHeader header {
        RecordType::JITCodeLoad,
        0,
        0,
    };
    uint32_t pid { 0 };
    uint32_t tid { 0 };
    uint64_t vma { 0 };
    uint64_t codeAddress { 0 };
    uint64_t codeSize { 0 };
    uint64_t codeIndex { 0 };
};

struct DebugInfoRecord {
    RecordHeader header {
        RecordType::JITCodeDebugInfo,
        0,
        0,
    };
    uint64_t codeAddress { 0 };
    uint64_t nrEntry { 0 };
};

struct DebugEntry {
    uint64_t codeAddress { 0 };
    uint32_t line { 0 };
    uint32_t discrim { 0 };
};

} // namespace JITDump

WTF_MAKE_TZONE_ALLOCATED_IMPL(PerfLog);

PerfLog& PerfLog::singleton()
{
    static LazyNeverDestroyed<PerfLog> logger;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        logger.construct();
    });
    return logger.get();
}

PerfLog::PerfLog()
{
    {
        m_file = FileSystem::createDumpFile(makeString("jit-"_s, ProfilerSupport::getCurrentThreadID(), "-"_s, WTF::getCurrentProcessID()), ".dump"_s, String::fromUTF8(Options::jitDumpDirectory()));
        RELEASE_ASSERT(m_file);

        if (Options::useIRDump())
            m_irDumpDirectory = Options::irDumpDirectory();

        if (Options::useSourceCodeDump())
            m_sourceCodeDumpDirectory = Options::sourceCodeDumpDirectory();

#if OS(LINUX)
        // Linux perf command records this mmap operation in perf.data as a metadata to the JIT perf annotations.
        // We do not use this mmap-ed memory region actually.
        auto* marker = mmap(nullptr, pageSize(), PROT_READ | PROT_EXEC, MAP_PRIVATE, m_file.platformHandle(), 0);
        RELEASE_ASSERT(marker != MAP_FAILED);
#endif
    }

    JITDump::FileHeader header;
    header.timestamp = ProfilerSupport::generateTimestamp();
    header.pid = getCurrentProcessID();

    Locker locker { m_lock };
    write(locker, WTF::unsafeMakeSpan(std::bit_cast<uint8_t*>(&header), sizeof(JITDump::FileHeader)));
    flush(locker);
}

void PerfLog::write(const AbstractLocker&, std::span<const uint8_t> data)
{
    auto result = m_file.write(data);
    RELEASE_ASSERT(result && *result == data.size());
}

void PerfLog::flush(const AbstractLocker&)
{
    m_file.flush();
}

void PerfLog::log(const CString& name, MacroAssemblerCodeRef<LinkBufferPtrTag> code, std::unique_ptr<IRDumpDebugInfo>&& irDebugInfo, std::unique_ptr<SourceCodeDumpDebugInfo>&& sourceCodeDebugInfo)
{
    auto timestamp = ProfilerSupport::generateTimestamp();
    auto tid = ProfilerSupport::getCurrentThreadID();
    ProfilerSupport::singleton().queue().dispatch([name = name, code, tid, timestamp, irDebugInfo = WTF::move(irDebugInfo), sourceCodeDebugInfo = WTF::move(sourceCodeDebugInfo)] {
        PerfLog& logger = singleton();
        size_t size = code.size();
        auto* executableAddress = code.code().untaggedPtr<const uint8_t*>();
        if (!size) {
            dataLogLnIf(PerfLogInternal::verbose, "0 size record ", name, " ", RawPointer(executableAddress));
            return;
        }

        CString irFilePath;
        Vector<std::pair<uint32_t, uint32_t>> lineEntries;
        struct SourceEntry {
            uint32_t codeOffset;
            uint32_t line;
            uint32_t column;
            CString filePath;
        };
        Vector<SourceEntry> sourceEntries;

        if (irDebugInfo) {
            auto baseName = makeString("irdump-"_s, String::fromUTF8(irDebugInfo->functionName.span()), "-"_s, WTF::getCurrentProcessID(), "-"_s, timestamp);

            String filePath;
            FileSystem::FileHandle handle;
            const CString& irDumpDir = logger.m_irDumpDirectory;
            if (irDumpDir.isNull()) {
                auto result = FileSystem::openTemporaryFile(baseName, ".txt"_s);
                filePath = result.first;
                handle = WTF::move(result.second);
            } else {
                filePath = makeString(String::fromUTF8(irDumpDir.span()), FileSystem::pathSeparator, baseName, ".txt"_s);
                handle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Truncate);
            }

            if (handle) {
                // Write sequential IR dump file from irLines.
                for (auto& irLine : irDebugInfo->irLines) {
                    CString line;
                    if (irLine.opName)
                        line = toCString("  ", irLine.opName, "\n");
                    else
                        line = toCString("BB#", irLine.blockIndex, "\n");
                    handle.write(WTF::asByteSpan(line.span()));
                }
                handle.flush();
                irFilePath = FileSystem::fileSystemRepresentation(filePath);

                // Convert code entries to (codeOffset, 1-based lineNumber) pairs.
                for (auto& codeEntry : irDebugInfo->codeEntries)
                    lineEntries.append({ codeEntry.codeOffset, codeEntry.irLineIndex + 1 });
            }
        }

        if (sourceCodeDebugInfo) {
            const CString& sourceCodeDumpDir = logger.m_sourceCodeDumpDirectory;
            for (auto& entry : sourceCodeDebugInfo->codeEntries) {
                CString filePath = protect(entry.sourceProvider)->sourceCodeDumpFilePath(sourceCodeDumpDir);
                if (!filePath.isNull())
                    sourceEntries.append({ entry.codeOffset, entry.lineColumn.line, entry.lineColumn.column, WTF::move(filePath) });
            }
        }

        Locker locker { logger.m_lock };

        if (!irFilePath.isNull() && !lineEntries.isEmpty()) {
            JITDump::DebugInfoRecord debugRecord;
            debugRecord.header.timestamp = timestamp;
            debugRecord.codeAddress = std::bit_cast<uintptr_t>(executableAddress);
            debugRecord.nrEntry = lineEntries.size();

            uint32_t totalSize = sizeof(JITDump::DebugInfoRecord);
            for (size_t i = 0; i < lineEntries.size(); ++i)
                totalSize += sizeof(JITDump::DebugEntry) + (irFilePath.length() + 1);
            debugRecord.header.totalSize = totalSize;

            logger.write(locker, unsafeMakeSpan(std::bit_cast<char*>(&debugRecord), sizeof(JITDump::DebugInfoRecord)));

            for (auto& [codeOffset, lineNumber] : lineEntries) {
                JITDump::DebugEntry debugEntry;
                debugEntry.codeAddress = std::bit_cast<uintptr_t>(executableAddress) + codeOffset;
                debugEntry.line = lineNumber;
                debugEntry.discrim = 0;
                logger.write(locker, unsafeMakeSpan(std::bit_cast<char*>(&debugEntry), sizeof(JITDump::DebugEntry)));
                logger.write(locker, irFilePath.spanIncludingNullTerminator());
            }
        }

        if (!sourceEntries.isEmpty()) {
            JITDump::DebugInfoRecord debugRecord;
            debugRecord.header.timestamp = timestamp;
            debugRecord.codeAddress = std::bit_cast<uintptr_t>(executableAddress);
            debugRecord.nrEntry = sourceEntries.size();

            uint32_t totalSize = sizeof(JITDump::DebugInfoRecord);
            for (auto& sourceEntry : sourceEntries)
                totalSize += sizeof(JITDump::DebugEntry) + (sourceEntry.filePath.length() + 1);
            debugRecord.header.totalSize = totalSize;

            logger.write(locker, unsafeMakeSpan(std::bit_cast<char*>(&debugRecord), sizeof(JITDump::DebugInfoRecord)));

            for (auto& sourceEntry : sourceEntries) {
                JITDump::DebugEntry debugEntry;
                debugEntry.codeAddress = std::bit_cast<uintptr_t>(executableAddress) + sourceEntry.codeOffset;
                debugEntry.line = sourceEntry.line;
                debugEntry.discrim = sourceEntry.column;
                logger.write(locker, unsafeMakeSpan(std::bit_cast<char*>(&debugEntry), sizeof(JITDump::DebugEntry)));
                logger.write(locker, sourceEntry.filePath.spanIncludingNullTerminator());
            }
        }

        JITDump::CodeLoadRecord record;
        record.header.timestamp = timestamp;
        record.header.totalSize = sizeof(JITDump::CodeLoadRecord) + (name.length() + 1) + size;
        record.pid = getCurrentProcessID();
        record.tid = tid;
        record.vma = std::bit_cast<uintptr_t>(executableAddress);
        record.codeAddress = std::bit_cast<uintptr_t>(executableAddress);
        record.codeSize = size;
        record.codeIndex = logger.m_codeIndex++;

        logger.write(locker, unsafeMakeSpan(std::bit_cast<char*>(&record), sizeof(JITDump::CodeLoadRecord)));
        logger.write(locker, name.spanIncludingNullTerminator());
        logger.write(locker, unsafeMakeSpan(executableAddress, size));
        logger.flush(locker);

        dataLogLnIf(PerfLogInternal::verbose, name, " [", record.codeIndex, "] ", RawPointer(executableAddress), "-", RawPointer(executableAddress + size), " ", size);
    });
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(ASSEMBLER)
