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

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheData.h"
#include <functional>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace NetworkCache {

class IOChannel : public ThreadSafeRefCounted<IOChannel> {
public:
    enum class Type { Read, Write, Create };
    static Ref<IOChannel> open(const String& file, Type);

    void read(size_t offset, size_t, std::function<void (Data&, int error)>);
    void readSync(size_t offset, size_t, std::function<void (Data&, int error)>);
    void write(size_t offset, const Data&, std::function<void (int error)>);

    int fileDescriptor() const { return m_fileDescriptor; }

private:
    IOChannel(int fd);

#if PLATFORM(COCOA)
    DispatchPtr<dispatch_io_t> m_dispatchIO;
#endif
    int m_fileDescriptor { 0 };
};

}
}

#endif
#endif
