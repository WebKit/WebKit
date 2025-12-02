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

#include "Test.h"
#include <WebCore/SharedMemory.h>
#include <limits>

#if PLATFORM(COCOA)
#include <mach/mach.h>
#endif

namespace WebCore {

void PrintTo(SharedMemory::Protection protection, ::std::ostream* o)
{
    if (protection == SharedMemory::Protection::ReadOnly)
        *o << "ReadOnly";
    else if (protection == SharedMemory::Protection::ReadWrite)
        *o << "ReadWrite";
    else
        *o << "Unknown";
}

}

namespace TestWebKitAPI {

using namespace WebCore;

enum class MemorySource {
    Malloc,
    SharedMemory,
#if PLATFORM(COCOA)
    ExplicitMapping
#endif
};

void PrintTo(MemorySource source, ::std::ostream* o)
{
    if (source == MemorySource::Malloc)
        *o << "Malloc";
    else if (source == MemorySource::SharedMemory)
        *o << "SharedMemory";
#if PLATFORM(COCOA)
    else if (source == MemorySource::ExplicitMapping)
        *o << "ExplicitMapping";
#endif
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

class VMAllocSpan {
public:
    VMAllocSpan() = default;
    VMAllocSpan(std::span<uint8_t> data)
        : m_data(data)
    {
    }
    VMAllocSpan(VMAllocSpan&& other)
    {
        *this = WTFMove(other);
    }
    VMAllocSpan& operator=(VMAllocSpan&& other)
    {
        if (this != &other)
            m_data = std::exchange(other.m_data, { });
        return *this;
    }
    ~VMAllocSpan()
    {
        if (m_data.empty())
            return;
        kern_return_t kr = vm_deallocate(mach_task_self(), reinterpret_cast<uintptr_t>(m_data.data()), m_data.size());
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
    }
private:
    std::span<uint8_t> m_data;
};

}
#endif

class SharedMemoryFromMemoryTest : public testing::TestWithParam<std::tuple<uint64_t, size_t, MemorySource, SharedMemory::Protection>> {
public:
    auto memorySize() const { return std::get<0>(GetParam()); }
    auto offset() const { return std::get<1>(GetParam()); }
    auto memorySource() const { return std::get<2>(GetParam()); }
    auto protection() const { return std::get<3>(GetParam()); }
    std::span<uint8_t> allocate();
protected:
    static constexpr uint64_t sizeOkToSkip = sizeof(size_t) == 4 ? 2 * GB : 4 * GB;
#if PLATFORM(COCOA)
    using Source = std::variant<std::unique_ptr<uint8_t[]>, RefPtr<SharedMemory>, VMAllocSpan>;
#else
    using Source = std::variant<std::unique_ptr<uint8_t[]>, RefPtr<SharedMemory>>;
#endif
    Source m_source;
};

std::span<uint8_t> SharedMemoryFromMemoryTest::allocate()
{
    auto size = static_cast<size_t>(memorySize()) + offset();
    std::span<uint8_t> data;
    if (memorySource() == MemorySource::Malloc) {
        if (auto source = std::unique_ptr<uint8_t[]> { new (std::nothrow) uint8_t[size] }) {
            data = { source.get(), size };
            m_source = WTFMove(source);
        }
    }
    if (memorySource() == MemorySource::SharedMemory) {
        if (auto shm = SharedMemory::allocate(size)) {
            data = shm->mutableSpan();
            m_source = WTFMove(shm);
        }
    }
#if PLATFORM(COCOA)
    if (memorySource() == MemorySource::ExplicitMapping) {
        // Try to create a more complex vm region. The intention would be to to get multiple kernel-side
        // memory objects.
        // 1. allocate a full region
        // 2. allocate one page at the start of the full region as named memory.
        vm_address_t dataAddress = 0;
        vm_prot_t vmProtection = VM_PROT_READ | VM_PROT_WRITE;
        size = std::max(size, static_cast<size_t>(vm_page_size));
        kern_return_t kr = vm_map(mach_task_self(), &dataAddress, size, 0, VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE, 0, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        memory_object_size_t memoryObjectSize = vm_page_size;
        mach_port_t port = MACH_PORT_NULL;
        kr = mach_make_memory_entry_64(mach_task_self(), &memoryObjectSize, 0, vmProtection | MAP_MEM_NAMED_CREATE, &port, MACH_PORT_NULL);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        kr = vm_map(mach_task_self(), &dataAddress, vm_page_size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, port, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        kr = mach_port_deallocate(mach_task_self(), port);
        ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
        data = { static_cast<uint8_t*>(reinterpret_cast<void*>(static_cast<uintptr_t>(dataAddress))), size };
        m_source = VMAllocSpan { data };
    }
#endif
    return data.subspan(offset(), static_cast<size_t>(memorySize()));
}

// Tests creating shared memory from a VM region.
// Tests that:
//   * The changes made to the VM region are visible through the shared memory object.
//   * The changes made through the shared memory object are visible to the original.
TEST_P(SharedMemoryFromMemoryTest, CreateHandleFromMemory)
{
    if (memorySize() > std::numeric_limits<size_t>::max())
        return;
    auto data = allocate();
    if (data.empty() && memorySize() >= sizeOkToSkip)
        return;
    ASSERT_FALSE(data.empty());
    ASSERT_EQ(data.size(), memorySize());
    fillTestPattern(data, 1);
    expectTestPattern(data, 1);

    auto handle = SharedMemory::Handle::createVMShare(data, protection());
#if !PLATFORM(COCOA)
    // Remove when implemented.
    if (!handle.has_value())
        return;
    ASSERT_FALSE(handle.has_value());
#endif
    ASSERT_TRUE(handle.has_value());
    auto shm2 = SharedMemory::map(WTFMove(*handle), protection());
    ASSERT_NOT_NULL(shm2);
    auto data2 = shm2->mutableSpan();
    expectTestPattern(data2, 1);
    EXPECT_NE(data.data(), data2.data());
    // Modify the orginal VM region and observe that the modification is visible
    // through the shared object.
    fillTestPattern(data, 2);
    expectTestPattern(data2, 2);
    if (protection() == SharedMemory::Protection::ReadWrite) {
        // Modify through the shared object and observe that the change is visible
        // in the original VM region.
        fillTestPattern(data2, 3);
        expectTestPattern(data2, 3);
        expectTestPattern(data, 3);
    }
}

// Tests creating shared memory from a VM copy of a VM region.
// Tests that:
//   * The changes made to the VM region are not visible through the shared memory object.
//   * The changes made through the shared memory object are not visible to the original.
TEST_P(SharedMemoryFromMemoryTest, CreateHandleVMCopyFromMemory)
{
    if (memorySize() > std::numeric_limits<size_t>::max())
        return;
    auto data = allocate();
    if (data.empty() && memorySize() >= sizeOkToSkip)
        return;
    ASSERT_FALSE(data.empty());
    ASSERT_EQ(data.size(), memorySize());
    fillTestPattern(data, 1);
    expectTestPattern(data, 1);

    auto handle = SharedMemory::Handle::createVMCopy(data, protection());
#if !PLATFORM(COCOA)
    // Remove when implemented.
    if (!handle.has_value())
        return;
    ASSERT_FALSE(handle.has_value());
#endif
    ASSERT_TRUE(handle.has_value());
    auto shm2 = SharedMemory::map(WTFMove(*handle), protection());
    ASSERT_NOT_NULL(shm2);
    auto data2 = shm2->mutableSpan();
    expectTestPattern(data2, 1);
    fillTestPattern(data, 2);
    expectTestPattern(data2, 1);
    if (protection() == SharedMemory::Protection::ReadWrite) {
        fillTestPattern(data2, 3);
        expectTestPattern(data2, 3);
        expectTestPattern(data, 2);
    }
}

// Tests creating shared memory from a physical copy of a VM region.
// Tests that:
//   * The changes made to the VM region are not visible through the shared memory object.
//   * The changes made through the shared memory object are not visible to the original.
TEST_P(SharedMemoryFromMemoryTest, CreateHandleCopyFromMemory)
{
    if (memorySize() > std::numeric_limits<size_t>::max())
        return;
    auto data = allocate();
    if (data.empty() && memorySize() >= sizeOkToSkip)
        return;
    ASSERT_FALSE(data.empty());
    ASSERT_EQ(data.size(), memorySize());
    fillTestPattern(data, 1);
    expectTestPattern(data, 1);

    auto handle = SharedMemory::Handle::createCopy(data, protection());
    ASSERT_TRUE(handle.has_value());
    auto shm2 = SharedMemory::map(WTFMove(*handle), protection());
    ASSERT_NOT_NULL(shm2);
    auto data2 = shm2->mutableSpan();
    expectTestPattern(data2, 1);
    fillTestPattern(data, 2);
    expectTestPattern(data2, 1);
    if (protection() == SharedMemory::Protection::ReadWrite) {
        fillTestPattern(data2, 3);
        expectTestPattern(data2, 3);
        expectTestPattern(data, 2);
    }
}

#if PLATFORM(COCOA)
#define ANY_MEMORY_SOURCE testing::Values(MemorySource::Malloc, MemorySource::SharedMemory, MemorySource::ExplicitMapping)
#else
#define ANY_MEMORY_SOURCE testing::Values(MemorySource::Malloc, MemorySource::SharedMemory)
#endif

INSTANTIATE_TEST_SUITE_P(SharedMemoryTest,
    SharedMemoryFromMemoryTest,
    testing::Combine(
        testing::Values(1, 2, KB, 100 * KB, 500 * MB, 4 * GB + 1, 20 * GB),
        testing::Values(0, 1, 444, 4097),
        ANY_MEMORY_SOURCE,
        testing::Values(SharedMemory::Protection::ReadOnly, SharedMemory::Protection::ReadWrite)),
    TestParametersToStringFormatter());

}
