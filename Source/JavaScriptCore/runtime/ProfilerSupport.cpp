/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include <wtf/MonotonicTime.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>

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
{
    StringPrintStream filename;
    if (auto* optionalDirectory = Options::textMarkersDirectory())
        filename.print(optionalDirectory);
    else
        filename.print("/tmp");
    filename.print("/marker-", getCurrentProcessID(), ".txt");
    auto* file = fopen(filename.toCString().data(), "wb");
    RELEASE_ASSERT(file);
    m_file = makeUnique<FilePrintStream>(file, FilePrintStream::Adopt);
    RELEASE_ASSERT(m_file);
}

static void write(const AbstractLocker&, FilePrintStream& stream, uint64_t start, uint64_t end, const CString& message)
{
    stream.println(start, " ", end, " ", message);
}

void ProfilerSupport::markStart(const void* identifier, Category category, const CString& message)
{
    UNUSED_PARAM(identifier);
    if (!Options::useTextMarkers())
        return;

    auto& profiler = singleton();

    Locker locker { profiler.m_lock };
    auto& table = profiler.m_markers[static_cast<unsigned>(category)];
    table.add(message, generateTimestamp());
}

void ProfilerSupport::markEnd(const void* identifier, Category category, const CString& message)
{
    UNUSED_PARAM(identifier);
    if (!Options::useTextMarkers())
        return;

    auto& profiler = singleton();

    Locker locker { profiler.m_lock };
    auto& table = profiler.m_markers[static_cast<unsigned>(category)];

    uint64_t start = generateTimestamp();
    uint64_t end = start;

    auto iterator = table.find(message);
    if (iterator != table.end()) {
        start = iterator->value;
        table.remove(iterator);
    }

    write(locker, *profiler.m_file, start, end, message);
}

void ProfilerSupport::mark(const void* identifier, Category category, const CString& message)
{
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(category);
    if (!Options::useTextMarkers())
        return;

    auto& profiler = singleton();

    Locker locker { profiler.m_lock };
    uint64_t timestamp = generateTimestamp();
    write(locker, *profiler.m_file, timestamp, timestamp, message);
}

} // namespace JSC
