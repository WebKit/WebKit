/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ProfilerSupport.h"

#include "Options.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/FileSystem.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>
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

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProfilerSupport);

uint64_t ProfilerSupport::generateTimestamp()
{
    return MonotonicTime::now().secondsSinceEpoch().nanosecondsAs<uint64_t>();
}

ProfilerSupport& ProfilerSupport::singleton()
{
    static LazyNeverDestroyed<ProfilerSupport> profiler;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        profiler.construct();
    });
    return profiler.get();
}

ProfilerSupport::ProfilerSupport()
    : m_queue(WorkQueue::create("JSC PerfLog"_s))
{
    if (Options::useTextMarkers()) {
        StringPrintStream filename;
        if (auto* optionalDirectory = Options::textMarkersDirectory())
            filename.print(optionalDirectory);
        else
            filename.print("/tmp");
        filename.print("/marker-", getCurrentProcessID(), ".txt");

        m_fd = open(filename.toCString().data(), O_CREAT | O_TRUNC | O_RDWR, 0666);
        RELEASE_ASSERT(m_fd != -1);

#if OS(LINUX)
        // Linux perf command records this mmap operation in perf.data as a metadata to the JIT perf annotations.
        // We do not use this mmap-ed memory region actually.
        auto* marker = mmap(nullptr, pageSize(), PROT_READ | PROT_EXEC, MAP_PRIVATE, m_fd, 0);
        RELEASE_ASSERT(marker != MAP_FAILED);
#endif

        auto* file = fdopen(m_fd, "wb");
        RELEASE_ASSERT(file);

        m_fileStream = makeUnique<FilePrintStream>(file, FilePrintStream::Adopt);
        RELEASE_ASSERT(m_fileStream);
    }
}

void ProfilerSupport::write(const AbstractLocker&, uint64_t start, uint64_t end, const CString& message)
{
    m_fileStream->println(start, " ", end, " ", message);
    m_fileStream->flush();
}

void ProfilerSupport::markStart(const void* identifier, Category category, CString&&)
{
    if (!Options::useTextMarkers())
        return;
    if (!identifier)
        return;

    auto& profiler = singleton();

    Locker locker { profiler.m_tableLock };
    auto& table = profiler.m_markers[static_cast<unsigned>(category)];
    table.add(identifier, generateTimestamp());
}

void ProfilerSupport::markEnd(const void* identifier, Category category, CString&& message)
{
    if (!Options::useTextMarkers())
        return;
    if (!identifier)
        return;

    auto timestamp = generateTimestamp();
    uint64_t start = timestamp;
    uint64_t end = timestamp;

    auto& profiler = singleton();
    {
        Locker locker { profiler.m_tableLock };
        auto& table = profiler.m_markers[static_cast<unsigned>(category)];

        auto iterator = table.find(identifier);
        if (iterator != table.end()) {
            start = iterator->value;
            table.remove(iterator);
        }
    }

    profiler.queue().dispatch([message = WTFMove(message), start, end] {
        auto& profiler = singleton();
        Locker locker { profiler.m_lock };
        profiler.write(locker, start, end, message);
    });
}

void ProfilerSupport::mark(const void* identifier, Category, CString&& message)
{
    if (!Options::useTextMarkers())
        return;
    if (!identifier)
        return;

    auto timestamp = generateTimestamp();
    singleton().queue().dispatch([message = WTFMove(message), timestamp] {
        auto& profiler = singleton();
        Locker locker { profiler.m_lock };
        profiler.write(locker, timestamp, timestamp, message);
    });
}


void ProfilerSupport::markInterval(const void* identifier, Category, MonotonicTime startTime, MonotonicTime endTime, CString&& message)
{
    if (!Options::useTextMarkers())
        return;
    if (!identifier)
        return;

    uint64_t start = startTime.secondsSinceEpoch().nanosecondsAs<uint64_t>();
    uint64_t end = endTime.secondsSinceEpoch().nanosecondsAs<uint64_t>();

    auto& profiler = singleton();
    profiler.queue().dispatch([message = WTFMove(message), start, end] {
        auto& profiler = singleton();
        Locker locker { profiler.m_lock };
        profiler.write(locker, start, end, message);
    });
}

} // namespace JSC
