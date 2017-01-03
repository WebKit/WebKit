/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WasmMemory.h"

#if ENABLE(WEBASSEMBLY)

#include <wtf/HexNumber.h>
#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Wasm {

namespace {
const bool verbose = false;
}

void Memory::dump(PrintStream& out) const
{
    String memoryHex;
    WTF::appendUnsigned64AsHex((uint64_t)(uintptr_t)m_memory, memoryHex);
    out.print("Memory at 0x", memoryHex, ", size ", m_size, "B capacity ", m_mappedCapacity, "B, initial ", m_initial, " maximum ", m_maximum, " mode ", makeString(m_mode));
}

const char* Memory::makeString(Mode mode) const
{
    switch (mode) {
    case Mode::BoundsChecking: return "BoundsChecking";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return "";
}

static_assert(sizeof(uint64_t) == sizeof(size_t), "We rely on allowing the maximum size of Memory we map to be 2^32 which is larger than fits in a 32-bit integer that we'd pass to mprotect if this didn't hold.");

Memory::Memory(PageCount initial, PageCount maximum, bool& failed)
    : m_size(initial.bytes())
    , m_initial(initial)
    , m_maximum(maximum)
    , m_mode(Mode::BoundsChecking)
    // FIXME: If we add signal based bounds checking then we need extra space for overflow on load.
    // see: https://bugs.webkit.org/show_bug.cgi?id=162693
{
    RELEASE_ASSERT(!maximum || maximum >= initial); // This should be guaranteed by our caller.

    m_mappedCapacity = maximum ? maximum.bytes() : PageCount::max().bytes();
    if (!m_mappedCapacity) {
        // This means we specified a zero as maximum (which means we also have zero as initial size).
        RELEASE_ASSERT(m_size == 0);
        m_memory = nullptr;
        m_mappedCapacity = 0;
        failed = false;
        if (verbose)
            dataLogLn("Memory::Memory allocating nothing ", *this);
        return;
    }

    // FIXME: It would be nice if we had a VM tag for wasm memory. https://bugs.webkit.org/show_bug.cgi?id=163600
    void* result = Options::simulateWebAssemblyLowMemory() ? MAP_FAILED : mmap(nullptr, m_mappedCapacity, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (result == MAP_FAILED) {
        // Try again with a different number.
        if (verbose)
            dataLogLn("Memory::Memory mmap failed once for capacity, trying again", *this);
        m_mappedCapacity = m_size;
        if (!m_mappedCapacity) {
            m_memory = nullptr;
            failed = false;
            if (verbose)
                dataLogLn("Memory::Memory mmap not trying again because size is zero ", *this);
            return;
        }

        result = mmap(nullptr, m_mappedCapacity, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (result == MAP_FAILED) {
            if (verbose)
                dataLogLn("Memory::Memory mmap failed twice ", *this);
            failed = true;
            return;
        }
    }

    ASSERT(m_size <= m_mappedCapacity);
    {
        bool success = !mprotect(result, static_cast<size_t>(m_size), PROT_READ | PROT_WRITE);
        RELEASE_ASSERT(success);
    }

    m_memory = result;
    failed = false;
    if (verbose)
        dataLogLn("Memory::Memory mmap succeeded ", *this);
}

Memory::~Memory()
{
    if (verbose)
        dataLogLn("Memory::~Memory ", *this);
    if (m_memory) {
        if (munmap(m_memory, m_mappedCapacity))
            CRASH();
    }
}

bool Memory::grow(PageCount newSize)
{
    RELEASE_ASSERT(newSize > PageCount::fromBytes(m_size));

    if (verbose)
        dataLogLn("Memory::grow to ", newSize, " from ", *this);

    if (maximum() && newSize > maximum())
        return false;

    uint64_t desiredSize = newSize.bytes();

    if (m_memory && desiredSize <= m_mappedCapacity) {
        bool success = !mprotect(static_cast<uint8_t*>(m_memory) + m_size, static_cast<size_t>(desiredSize - m_size), PROT_READ | PROT_WRITE);
        RELEASE_ASSERT(success);
        m_size = desiredSize;
        if (verbose)
            dataLogLn("Memory::grow in-place ", *this);
        return true;
    }

    // Otherwise, let's try to make some new memory.
    void* newMemory = mmap(nullptr, desiredSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (newMemory == MAP_FAILED)
        return false;

    if (m_memory) {
        memcpy(newMemory, m_memory, m_size);
        bool success = !munmap(m_memory, m_mappedCapacity);
        RELEASE_ASSERT(success);
    }
    m_memory = newMemory;
    m_mappedCapacity = desiredSize;
    m_size = desiredSize;

    if (verbose)
        dataLogLn("Memory::grow ", *this);
    return true;
}

} // namespace JSC

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
