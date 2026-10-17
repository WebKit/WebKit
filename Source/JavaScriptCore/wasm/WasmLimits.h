/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/PageCount.h>
#include <JavaScriptCore/WasmAddressType.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <wtf/MathExtras.h>

namespace JSC {

namespace Wasm {

// These limits are arbitrary except that they match the limits imposed
// by other browsers' implementation of WebAssembly. It is desirable for
// us to accept at least the same inputs.
// see https://www.w3.org/TR/wasm-js-api-2/#limits

constexpr size_t maxTypes = 1000000;
constexpr size_t maxFunctions = 1000000;
constexpr size_t maxImports = 1000000;
constexpr size_t maxExports = 1000000;
constexpr size_t maxExceptions = 1000000;
constexpr size_t maxGlobals = 1000000;
constexpr size_t maxDataSegments = 100000;
constexpr size_t maxMemories = 100;
constexpr size_t maxStructFieldCount = 10000;
constexpr size_t maxArrayNewFixedArgs = 10000;
constexpr size_t maxRecursionGroupCount = 1000000;
constexpr size_t maxNumberOfRecursionGroups = 1000000;
constexpr size_t maxSubtypeSupertypeCount = 1;
constexpr size_t maxSubtypeDepth = 63;

constexpr size_t maxModuleSize = 1024 * 1024 * 1024;
constexpr size_t maxFunctionSize = 7654321;
constexpr size_t maxFunctionLocals = 50000;
constexpr size_t maxFunctionParams = 1000;
constexpr size_t maxFunctionReturns = 1000;

constexpr size_t maxTableEntries = 10000000;
constexpr unsigned maxTables = 100000;

constexpr uint64_t maxMemory32Pages = PageCount::maxMemory32PageCount;
constexpr uint64_t maxMemory64Pages = PageCount::maxPageCount;

// The number of 2**log2PageSize-byte pages spanning a 2**log2AddressSpaceBytes-byte address space,
// saturating when that count does not fit in a uint64_t.
constexpr uint64_t pagesSpanningAddressSpace(unsigned log2AddressSpaceBytes, uint8_t log2PageSize)
{
    if (log2PageSize > log2AddressSpaceBytes)
        return 0;
    const unsigned log2Pages = log2AddressSpaceBytes - log2PageSize;
    if (log2Pages >= 64)
        return std::numeric_limits<uint64_t>::max();
    return 1ULL << log2Pages;
}

constexpr uint64_t maxDeclarablePages(AddressType addressType, uint8_t log2PageSize)
{
    if (addressType.is64Bit()) {
        // FIXME: A PageCount still counts 64 KiB pages, and its invalid sentinel is UINT64_MAX, so
        // a small-page i64 memory cannot yet declare the 2**64 pages the proposal allows. Lift this
        // to pagesSpanningAddressSpace(64, log2PageSize) once PageCount carries its own page size.
        return std::min<uint64_t>(pagesSpanningAddressSpace(64, log2PageSize), PageCount::maxPageCount);
    }
    // An i32 memory's limits must fit in a u32 as well as in its address space.
    return std::min<uint64_t>(pagesSpanningAddressSpace(32, log2PageSize), std::numeric_limits<uint32_t>::max());
}

static_assert(maxDeclarablePages(AddressType { AddressType::I32 }, WTF::fastLog2(PageCount::pageSize)) == maxMemory32Pages);
static_assert(maxDeclarablePages(AddressType { AddressType::I64 }, WTF::fastLog2(PageCount::pageSize)) == maxMemory64Pages);
static_assert(maxDeclarablePages(AddressType { AddressType::I32 }, 0) == (1ULL << 32) - 1);
static_assert(maxDeclarablePages(AddressType { AddressType::I64 }, 0) <= PageCount::maxPageCount);

// MAX_ARRAY_BUFFER_SIZE is not a page multiple on 32-bit, and a memory's size always is.
constexpr uint64_t maxPageAlignedArrayBufferBytes = (MAX_ARRAY_BUFFER_SIZE / PageCount::pageSize) * PageCount::pageSize;

// The largest byte length the buffer of a memory of this address type may advertise. This is a
// property of the address type alone: the page size only changes how many pages span those bytes.
constexpr uint64_t maxBufferByteLength(AddressType addressType)
{
    if (addressType.is64Bit())
        return maxPageAlignedArrayBufferBytes;
    return std::min<uint64_t>(PageCount::maxMemory32Bytes, maxPageAlignedArrayBufferBytes);
}

#if USE(LARGE_TYPED_ARRAYS)
static_assert(maxBufferByteLength(AddressType { AddressType::I32 }) == maxMemory32Pages * PageCount::pageSize);
static_assert(maxBufferByteLength(AddressType { AddressType::I64 }) == maxPageAlignedArrayBufferBytes);
#endif

// Limit of GC arrays in bytes. This is not included in the limits in the
// JS API spec, but we set a limit to avoid complicated boundary conditions.
constexpr size_t maxArraySizeInBytes = 1 << 30;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
