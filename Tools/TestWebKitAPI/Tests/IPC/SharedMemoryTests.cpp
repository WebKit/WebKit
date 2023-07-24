/*
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

#include "config.h"

#include "SharedMemory.h"
#include "Test.h"
#include <limits>

#if PLATFORM(COCOA)
#include <mach/mach.h>
#endif
namespace WebKit {

void PrintTo(WebKit::SharedMemory::Protection protection, ::std::ostream* o)
{
    if (protection == WebKit::SharedMemory::Protection::ReadOnly)
        *o << "ReadOnly";
    else if (protection == WebKit::SharedMemory::Protection::ReadWrite)
        *o << "ReadWrite";
    else
        *o << "Unknown";
}
}

namespace TestWebKitAPI {

enum class MemorySource {
    Malloc,
    SharedMemory,
    ExplicitMapping
};

void PrintTo(MemorySource source, ::std::ostream* o)
{
    if (source == MemorySource::Malloc)
        *o << "Malloc";
    else if (source == MemorySource::SharedMemory)
        *o << "SharedMemory";
    else if (source == MemorySource::ExplicitMapping)
        *o << "ExplicitMapping";
    else
        *o << "Unknown";
}

static void fillTestPattern(std::span<uint8_t> data, size_t seed)
{
    for (size_t i = 0; i < 5 && i < data.size(); ++i)
        data[i] = seed + i;
    if (data.size() < 12)
        return;
    for (size_t i = 1; i < 6; ++i)
        data[data.size() -  i] = seed + i + 77u;
    auto mid = data.size() / 2;
    data[mid] = seed + 99;
}

static void expectTestPattern(std::span<uint8_t> data, size_t seed)
{
    for (size_t i = 0; i < 5 && i < data.size(); ++i)
        EXPECT_EQ(data[i], static_cast<uint8_t>(seed + i));
    if (data.size() < 12)
        return;
    for (size_t i = 1; i < 6; ++i)
        EXPECT_EQ(data[data.size() -  i], static_cast<uint8_t>(seed + i + 77u));
    auto mid = data.size() / 2;
    EXPECT_EQ(data[mid], static_cast<uint8_t>(seed + 99));
}

#if PLATFORM(COCOA)
namespace {

struct VMAllocation {
    VMAllocation(uint8_t* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }
    VMAllocation(VMAllocation&& other)
    {
        *this = WTFMove(other);
    }
    VMAllocation& operator=(VMAllocation&& other)
    {
        if (this != &other) {
            m_data = std::exchange(other.m_data, nullptr);
            m_size = other.m_size;
        }
        return *this;
    }
    ~VMAllocation()
    {
        if (!m_data)
            return;
        kern_return_t kr = vm_deallocate(mach_task_self(), reinterpret_cast<uintptr_t>(m_data), m_size);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    }
    uint8_t* m_data { nullptr };
    size_t m_size { 0 };
};

}
#endif

class SharedMemoryFromMemoryTest : public testing::TestWithParam<std::tuple<uint64_t, size_t, MemorySource, WebKit::SharedMemory::Protection>> {
public:
    auto memorySize() const { return std::get<0>(GetParam()); }
    auto offset() const { return std::get<1>(GetParam()); }
    auto memorySource() const { return std::get<2>(GetParam()); }
    auto protection() const { return std::get<3>(GetParam()); }
    std::span<uint8_t> allocate();
protected:
    // TODO: on platforms pre-Ventura, 4GB+ fails.
    static constexpr uint64_t sizeOkToSkip = sizeof(size_t) == 4 ? 2 * GB : 4 * GB;
    std::variant<std::unique_ptr<uint8_t[]>, RefPtr<WebKit::SharedMemory>, VMAllocation> m_source;
};

std::span<uint8_t> SharedMemoryFromMemoryTest::allocate()
{
    auto size = static_cast<size_t>(memorySize()) + offset();
    uint8_t* data = nullptr;
    if (memorySource() == MemorySource::Malloc) {
        data = new (std::nothrow) uint8_t[size];
        if (data)
            m_source = std::unique_ptr<uint8_t[]> { data };
    }
    if (memorySource() == MemorySource::SharedMemory) {
        auto shm = WebKit::SharedMemory::allocate(size);
        if (shm) {
            data = static_cast<uint8_t*>(shm->data());
            m_source = WTFMove(shm);
        }
    }
    if (memorySource() == MemorySource::ExplicitMapping) {
#if PLATFORM(COCOA)
        // Try to create a more complex vm region. The intention would be to to get multiple kernel-side
        // memory objects.
        // 1. allocate a full region
        // 2. allocate one page at the start of the full region as named memory.
        vm_address_t dataAddress = 0;
        vm_prot_t vmProtection = VM_PROT_READ | VM_PROT_WRITE;
        size = std::max(size, static_cast<size_t>(vm_page_size));
        kern_return_t kr = vm_map(mach_task_self(), &dataAddress, size, 0, VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE, 0, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        data = static_cast<uint8_t*>(reinterpret_cast<void*>(static_cast<uintptr_t>(dataAddress)));
        memory_object_size_t memoryObjectSize = vm_page_size;
        mach_port_t port = MACH_PORT_NULL;
        kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, 0, vmProtection | MAP_MEM_NAMED_CREATE, &port, MACH_PORT_NULL);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        kr = vm_map(mach_task_self(), &dataAddress, vm_page_size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, port, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        kr = mach_port_deallocate(mach_task_self(), port);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        m_source = VMAllocation { data, size };
#endif
    }
    if (!data)
        return { };
    return { data + offset(), static_cast<size_t>(memorySize()) };
}

#if PLATFORM(COCOA)
TEST_P(SharedMemoryFromMemoryTest, CreateHandleFromMemory)
{
    if (memorySize() > std::numeric_limits<size_t>::max())
        return;
    auto data = allocate();
    if (data.empty() && memorySize() >= sizeOkToSkip)
        return;
    ASSERT_FALSE(data.empty());
    ASSERT_EQ(data.size(), memorySize());
    auto handle = WebKit::SharedMemory::Handle::create(data, protection());
    fillTestPattern(data, 1);
    expectTestPattern(data, 1);

    ASSERT_TRUE(handle.has_value());
    ASSERT_FALSE(handle->isNull());

    auto shm2 = WebKit::SharedMemory::map(WTFMove(*handle), protection());
    ASSERT_NOT_NULL(shm2);
    std::span<uint8_t> data2 { static_cast<uint8_t*>(shm2->data()), shm2->size() };
    expectTestPattern(data2, 1);
    if (protection() == WebKit::SharedMemory::Protection::ReadWrite) {
        fillTestPattern(data2, 2);
        expectTestPattern(data2, 2);
        expectTestPattern(data, 2);
    } else
        EXPECT_NE(data.data(), data2.data());
}
#endif

TEST_P(SharedMemoryFromMemoryTest, CreateHandleCopyFromMemory)
{
    if (memorySize() > std::numeric_limits<size_t>::max())
        return;
    auto data = allocate();
    if (data.empty() && memorySize() >= sizeOkToSkip)
        return;
#if !PLATFORM(COCOA)
    if (memorySource() == MemorySource::ExplicitMapping)
        return;
#endif
    ASSERT_FALSE(data.empty());
    ASSERT_EQ(data.size(), memorySize());
    fillTestPattern(data, 1);
    expectTestPattern(data, 1);
    auto handle = WebKit::SharedMemory::Handle::createCopy(data, protection());

    ASSERT_TRUE(handle.has_value());
    ASSERT_FALSE(handle->isNull());

    auto shm2 = WebKit::SharedMemory::map(WTFMove(*handle), protection());
    ASSERT_NOT_NULL(shm2);
    std::span<uint8_t> data2 { static_cast<uint8_t*>(shm2->data()), shm2->size() };
    expectTestPattern(data2, 1);
    if (protection() == WebKit::SharedMemory::Protection::ReadWrite) {
        fillTestPattern(data2, 2);
        expectTestPattern(data2, 2);
        expectTestPattern(data, 1);
    } else
        EXPECT_NE(data.data(), data2.data());
}

INSTANTIATE_TEST_SUITE_P(SharedMemoryTest,
    SharedMemoryFromMemoryTest,
    testing::Combine(
        testing::Values(1, 2, KB, 100 * KB, 500 * MB, 4 * GB + 1, 20 * GB),
        testing::Values(0, 1, 444, 4097),
        testing::Values(MemorySource::Malloc, MemorySource::SharedMemory, MemorySource::ExplicitMapping),
        testing::Values(WebKit::SharedMemory::Protection::ReadOnly, WebKit::SharedMemory::Protection::ReadWrite)),
    TestParametersToStringFormatter());

}
