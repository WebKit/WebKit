/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheIOChannel_h
#define NetworkCacheIOChannel_h

#include "NetworkCacheData.h"
#include <wtf/Function.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

#if USE(SOUP)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebKit {
namespace NetworkCache {

class IOChannel : public ThreadSafeRefCounted<IOChannel> {
public:
    enum class Type { Read, Write, Create };
    static Ref<IOChannel> open(const String& file, Type);

    // Using nullptr as queue submits the result to the main queue.
    // FIXME: We should add WorkQueue::main() instead.
    void read(size_t offset, size_t, WorkQueue*, Function<void (Data&, int error)>&&);
    void write(size_t offset, const Data&, WorkQueue*, Function<void (int error)>&&);

    const String& path() const { return m_path; }
    Type type() const { return m_type; }

#if !USE(SOUP)
    bool isOpened() const { return FileSystem::isHandleValid(m_fileDescriptor); }
#else
    bool isOpened() const { return true; }
#endif

    ~IOChannel();

private:
    IOChannel(const String& filePath, IOChannel::Type);

#if USE(SOUP)
    void readSyncInThread(size_t offset, size_t, WorkQueue*, Function<void (Data&, int error)>&&);
#endif

    String m_path;
    Type m_type;

#if !USE(SOUP)
    FileSystem::PlatformFileHandle m_fileDescriptor { FileSystem::invalidPlatformFileHandle };
#endif
    std::atomic<bool> m_wasDeleted { false }; // Try to narrow down a crash, https://bugs.webkit.org/show_bug.cgi?id=165659
#if PLATFORM(COCOA)
    OSObjectPtr<dispatch_io_t> m_dispatchIO;
#endif
#if USE(SOUP)
    GRefPtr<GInputStream> m_inputStream;
    GRefPtr<GOutputStream> m_outputStream;
    GRefPtr<GFileIOStream> m_ioStream;
#endif
};

}
}

#endif
