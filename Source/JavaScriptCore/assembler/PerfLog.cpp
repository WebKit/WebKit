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
#include "PerfLog.h"

#if ENABLE(ASSEMBLER) && OS(LINUX)

#include <elf.h>
#include <fcntl.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/DataLog.h>
#include <wtf/MonotonicTime.h>
#include <wtf/PageBlock.h>
#include <wtf/ProcessID.h>

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

#if CPU(X86)
static constexpr uint32_t elfMachine = EM_386;
#elif CPU(X86_64)
static constexpr uint32_t elfMachine = EM_X86_64;
#elif CPU(ARM64)
static constexpr uint32_t elfMachine = EM_AARCH64;
#elif CPU(ARM)
static constexpr uint32_t elfMachine = EM_ARM;
#elif CPU(MIPS)
#if CPU(LITTLE_ENDIAN)
static constexpr uint32_t elfMachine = EM_MIPS_RS3_LE;
#else
static constexpr uint32_t elfMachine = EM_MIPS;
#endif
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

} // namespace JITDump

PerfLog& PerfLog::singleton()
{
    static PerfLog* logger;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        logger = new PerfLog;
    });
    return *logger;
}

static inline uint64_t generateTimestamp()
{
    return MonotonicTime::now().secondsSinceEpoch().nanosecondsAs<uint64_t>();
}

static inline pid_t getCurrentThreadID()
{
    return static_cast<pid_t>(syscall(__NR_gettid));
}

PerfLog::PerfLog()
{
    {
        std::array<char, 1024> filename;
        snprintf(filename.data(), filename.size() - 1, "jit-%d.dump", getCurrentProcessID());
        filename[filename.size() - 1] = '\0';
        m_fd = open(filename.data(), O_CREAT | O_TRUNC | O_RDWR, 0666);
        RELEASE_ASSERT(m_fd != -1);

        // Linux perf command records this mmap operation in perf.data as a metadata to the JIT perf annotations.
        // We do not use this mmap-ed memory region actually.
        m_marker = mmap(nullptr, pageSize(), PROT_READ | PROT_EXEC, MAP_PRIVATE, m_fd, 0);
        RELEASE_ASSERT(m_marker != MAP_FAILED);

        m_file = fdopen(m_fd, "wb");
        RELEASE_ASSERT(m_file);
    }

    JITDump::FileHeader header;
    header.timestamp = generateTimestamp();
    header.pid = getCurrentProcessID();

    auto locker = holdLock(m_lock);
    write(locker, &header, sizeof(JITDump::FileHeader));
    flush(locker);
}

void PerfLog::write(const AbstractLocker&, const void* data, size_t size)
{
    size_t result = fwrite(data, 1, size, m_file);
    RELEASE_ASSERT(result == size);
}

void PerfLog::flush(const AbstractLocker&)
{
    fflush(m_file);
}

void PerfLog::log(CString&& name, const uint8_t* executableAddress, size_t size)
{
    if (!size) {
        dataLogLnIf(PerfLogInternal::verbose, "0 size record ", name, " ", RawPointer(executableAddress));
        return;
    }

    PerfLog& logger = singleton();
    auto locker = holdLock(logger.m_lock);

    JITDump::CodeLoadRecord record;
    record.header.timestamp = generateTimestamp();
    record.header.totalSize = sizeof(JITDump::CodeLoadRecord) + (name.length() + 1) + size;
    record.pid = getCurrentProcessID();
    record.tid = getCurrentThreadID();
    record.vma = bitwise_cast<uintptr_t>(executableAddress);
    record.codeAddress = bitwise_cast<uintptr_t>(executableAddress);
    record.codeSize = size;
    record.codeIndex = logger.m_codeIndex++;

    logger.write(locker, &record, sizeof(JITDump::CodeLoadRecord));
    logger.write(locker, name.data(), name.length() + 1);
    logger.write(locker, executableAddress, size);
    logger.flush(locker);

    dataLogLnIf(PerfLogInternal::verbose, name, " [", record.codeIndex, "] ", RawPointer(executableAddress), "-", RawPointer(executableAddress + size), " ", size);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && OS(LINUX)
