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
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

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
        m_file = FileSystem::createDumpFile(makeString("marker-"_s, getCurrentThreadID(), "-"_s, WTF::getCurrentProcessID()), ".txt"_s, String::fromUTF8(Options::textMarkersDirectory()));
        RELEASE_ASSERT(m_file);

#if OS(LINUX)
        // Linux perf command records this mmap operation in perf.data as a metadata to the JIT perf annotations.
        // We do not use this mmap-ed memory region actually.
        auto* marker = mmap(nullptr, pageSize(), PROT_READ | PROT_EXEC, MAP_PRIVATE, m_file.platformHandle(), 0);
        RELEASE_ASSERT(marker != MAP_FAILED);
#endif
    }
}

uint32_t ProfilerSupport::getCurrentThreadID()
{
#if OS(LINUX)
    return static_cast<uint32_t>(syscall(__NR_gettid));
#elif OS(DARWIN)
    // Ideally we would like to use pthread_threadid_np. But this is 64bit, while required one is 32bit.
    // For now, as a workaround, we only report lower 32bit of thread ID.
    uint64_t thread = 0;
    pthread_threadid_np(NULL, &thread);
    return static_cast<uint32_t>(thread);
#elif OS(WINDOWS)
    return static_cast<uint32_t>(GetCurrentThreadId());
#else
    return 0;
#endif
}

void ProfilerSupport::write(const AbstractLocker&, uint64_t start, uint64_t end, const CString& message)
{
    auto header = toCString(start, " ", end, " ");
    m_file.write(WTF::asByteSpan(header.span()));
    m_file.write(WTF::asByteSpan(message.span()));
    m_file.write(WTF::asByteSpan("\n"_span));
    m_file.flush();
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

    profiler.queue().dispatch([message = WTF::move(message), start, end] {
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
    singleton().queue().dispatch([message = WTF::move(message), timestamp] {
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
    profiler.queue().dispatch([message = WTF::move(message), start, end] {
        auto& profiler = singleton();
        Locker locker { profiler.m_lock };
        profiler.write(locker, start, end, message);
    });
}

void ProfilerSupport::dumpIonGraphFunction(const String& functionName, Ref<JSON::Object>&& function)
{
    if (!Options::dumpIonGraph())
        return;
    auto json = JSON::Object::create();
    auto functions = JSON::Array::create();
    functions->pushObject(WTF::move(function));
    json->setInteger("version"_s, 1);
    json->setArray("functions"_s, WTF::move(functions));
    auto string = json->toJSONString();

    auto handle = FileSystem::createDumpFile(makeString("iongraph-"_s, functionName, "-"_s, WTF::getCurrentProcessID(), "-"_s, generateTimestamp()), ".json"_s, String::fromUTF8(Options::ionGraphDirectory()));
    RELEASE_ASSERT(handle);
    handle.write(WTF::asByteSpan(string.utf8().span()));
    handle.flush();
}

} // namespace JSC
