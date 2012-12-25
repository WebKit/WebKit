/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MemoryUsageSupport_h
#define MemoryUsageSupport_h

#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MemoryUsageSupport {
public:
    // Returns the current space allocated for the pagefile, in MB.
    // That is committed size for Windows and virtual memory size for POSIX.
    static int memoryUsageMB();

    // Same as above, but always returns actual value, without any
    // caches.
    static int actualMemoryUsageMB();

    // If memory usage is below this threshold, do not bother forcing GC.
    static int lowMemoryUsageMB();

    // If memory usage is above this threshold, force GC more aggressively.
    static int highMemoryUsageMB();

    // Delta of memory usage growth (vs. last actualMemoryUsageMB())
    // to force GC when memory usage is high.
    static int highUsageDeltaMB();

    // Returns private and shared usage, in bytes. Private bytes is the amount of
    // memory currently allocated to this process that cannot be shared. Returns
    // false on platform specific error conditions.
    static bool processMemorySizesInBytes(size_t* privateBytes, size_t* sharedBytes);

    // A callback for requestProcessMemorySizes
    class ProcessMemorySizesCallback {
    public:
        virtual ~ProcessMemorySizesCallback() { }
        virtual void dataReceived(size_t privateBytes, size_t sharedBytes) = 0;
    };

    // Requests private and shared usage, in bytes. Private bytes is the amount of
    // memory currently allocated to this process that cannot be shared.
    static void requestProcessMemorySizes(PassOwnPtr<ProcessMemorySizesCallback> requestCallback);

    class ComponentInfo {
    public:
        ComponentInfo(const String& name, size_t size) : m_name(name), m_sizeInBytes(size) { }

        const String m_name;
        size_t m_sizeInBytes;
    };

    // Reports private memory used by components in bytes.
    static void memoryUsageByComponents(Vector<ComponentInfo>&);
};

} // namespace WebCore

#endif // MemoryUsageSupport_h
