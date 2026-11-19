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

#include "Helpers/Test.h"
#include <WebCore/SharedBuffer.h>
#include <WebCore/SharedMemory.h>
#include <limits>

#if PLATFORM(COCOA)
#include <mach/mach.h>
#include <mach/vm_purgable.h>
#include <wtf/spi/cocoa/MachVMSPI.h>
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

static void expectTestPattern(std::span<const uint8_t> data, size_t seed)
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
        *this = WTF::move(other);
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
            m_source = WTF::move(source);
        }
    }
    if (memorySource() == MemorySource::SharedMemory) {
        if (auto shm = SharedMemory::allocate(size)) {
            data = shm->mutableSpan();
            m_source = WTF::move(shm);
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
    auto shm2 = SharedMemory::map(WTF::move(*handle), protection());
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
    auto shm2 = SharedMemory::map(WTF::move(*handle), protection());
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
    auto shm2 = SharedMemory::map(WTF::move(*handle), protection());
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

#if OS(DARWIN)
// Tests mapping shared memory as CoW
// Tests that:
//   * Changes made to the original mapping aren't visible in the secondary mapping
//   * Changes made to the secondary mapping aren't visible in the original mapping
TEST_P(SharedMemoryFromMemoryTest, CreateCOWFromMemory)
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
    ASSERT_TRUE(handle.has_value());
    auto shm2 = SharedMemory::map(WTF::move(*handle), protection(), SharedMemory::CopyOnWrite::Yes);
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
#endif // OS(DARWIN)

#if HAVE(SAFE_MEMORY_TRANSFER)
static constexpr size_t transferSize = 16 * KB;

static std::optional<SharedMemory::Handle> createDonatedHandle(uint8_t seed, int purgeableState = VM_PURGABLE_NONVOLATILE)
{
    RefPtr source = SharedMemory::allocate(transferSize);
    if (!source)
        return std::nullopt;
    fillTestPattern(source->mutableSpan(), seed);
    int state = purgeableState;
    if (vm_purgable_control(mach_task_self(), reinterpret_cast<vm_address_t>(source->span().data()), VM_PURGABLE_SET_STATE, &state) != KERN_SUCCESS)
        return std::nullopt;
    return source->createHandle(SharedMemory::Protection::ReadOnly);
}

TEST(SharedMemoryTransfer, ExclusiveAfterDonation)
{
    auto handle = createDonatedHandle(1);
    ASSERT_TRUE(handle.has_value());

    RefPtr received = SharedMemory::mapTransfer(WTF::move(*handle));
    ASSERT_NOT_NULL(received);
    EXPECT_TRUE(received->isExclusive());
    expectTestPattern(received->span(), 1);

    Ref buffer = received->createIsolatedSharedBuffer(transferSize);
    EXPECT_EQ(buffer->span().data(), received->span().data());
}

TEST(SharedMemoryTransfer, NotExclusiveWhenMappedWithoutTransfer)
{
    auto handle = createDonatedHandle(1);
    ASSERT_TRUE(handle.has_value());

    RefPtr received = SharedMemory::map(WTF::move(*handle), SharedMemory::Protection::ReadOnly);
    ASSERT_NOT_NULL(received);
    handle = std::nullopt;
    EXPECT_FALSE(received->isExclusive());
}

TEST(SharedMemoryTransfer, NotExclusiveWhenSenderKeepsMapping)
{
    RefPtr source = SharedMemory::allocate(transferSize);
    ASSERT_NOT_NULL(source);
    fillTestPattern(source->mutableSpan(), 2);
    auto handle = source->createHandle(SharedMemory::Protection::ReadOnly);
    ASSERT_TRUE(handle.has_value());

    RefPtr received = SharedMemory::mapTransfer(WTF::move(*handle));
    ASSERT_NOT_NULL(received);
    EXPECT_FALSE(received->isExclusive());

    Ref buffer = received->createIsolatedSharedBuffer(transferSize);
    EXPECT_NE(buffer->span().data(), received->span().data());
    expectTestPattern(buffer->span(), 2);
}

TEST(SharedMemoryTransfer, NotExclusiveWhenSenderKeepsSendRight)
{
    auto handle = createDonatedHandle(3);
    ASSERT_TRUE(handle.has_value());
    SharedMemory::Handle retainedHandle { *handle };

    RefPtr received = SharedMemory::mapTransfer(WTF::move(*handle));
    ASSERT_NOT_NULL(received);
    EXPECT_FALSE(received->isExclusive());
}

TEST(SharedMemoryTransfer, NotExclusiveWhenPurgeable)
{
    for (int purgeableState : { VM_PURGABLE_VOLATILE, VM_PURGABLE_EMPTY }) {
        auto handle = createDonatedHandle(4, purgeableState);
        ASSERT_TRUE(handle.has_value());

        RefPtr received = SharedMemory::mapTransfer(WTF::move(*handle));
        ASSERT_NOT_NULL(received);
        EXPECT_FALSE(received->isExclusive()) << "purgeable state " << purgeableState;

        Ref buffer = received->createIsolatedSharedBuffer(transferSize);
        EXPECT_NE(buffer->span().data(), received->span().data());
    }
}

TEST(SharedMemoryTransfer, SubrangeOfPurgeableObject)
{
    for (int purgeableState : { VM_PURGABLE_NONVOLATILE, VM_PURGABLE_VOLATILE }) {
        std::optional<SharedMemory::Handle> handle;
        {
            RefPtr source = SharedMemory::allocate(4 * vm_page_size);
            ASSERT_NOT_NULL(source);
            fillTestPattern(source->mutableSpan(), 6);
            int state = purgeableState;
            ASSERT_EQ(vm_purgable_control(mach_task_self(), reinterpret_cast<vm_address_t>(source->span().data()), VM_PURGABLE_SET_STATE, &state), KERN_SUCCESS);
            handle = SharedMemory::Handle::createVMShare(source->span().subspan(vm_page_size, 2 * vm_page_size), SharedMemory::Protection::ReadOnly);
            ASSERT_TRUE(handle.has_value());
        }

        RefPtr received = SharedMemory::mapTransfer(WTF::move(*handle));
        ASSERT_NOT_NULL(received);
        EXPECT_EQ(received->isExclusive(), purgeableState == VM_PURGABLE_NONVOLATILE) << "purgeable state " << purgeableState;
    }
}

TEST(SharedMemoryTransfer, ExclusiveAfterForwarding)
{
    auto handle = createDonatedHandle(5);
    ASSERT_TRUE(handle.has_value());

    RefPtr firstHop = SharedMemory::mapTransfer(WTF::move(*handle));
    ASSERT_NOT_NULL(firstHop);
    EXPECT_TRUE(firstHop->isExclusive());

    auto forwardedHandle = firstHop->createHandle(SharedMemory::Protection::ReadOnly);
    ASSERT_TRUE(forwardedHandle.has_value());
    EXPECT_FALSE(firstHop->isExclusive());
    firstHop = nullptr;

    RefPtr secondHop = SharedMemory::mapTransfer(WTF::move(*forwardedHandle));
    ASSERT_NOT_NULL(secondHop);
    EXPECT_TRUE(secondHop->isExclusive());
    expectTestPattern(secondHop->span(), 5);
}

TEST(SharedMemoryTransfer, ExclusiveAfterSenderSetsLedger)
{
    auto handle = createDonatedHandle(10);
    ASSERT_TRUE(handle.has_value());
    handle->takeOwnershipOfMemory(MemoryLedger::Media);

    RefPtr received = SharedMemory::mapTransfer(WTF::move(*handle));
    ASSERT_NOT_NULL(received);
    EXPECT_TRUE(received->isExclusive());
    expectTestPattern(received->span(), 10);
}

static bool isInReadOnlyMapping(std::span<const uint8_t> data)
{
    auto start = reinterpret_cast<mach_vm_address_t>(data.data());
    mach_vm_address_t regionStart = start;
    mach_vm_size_t regionSize = 0;
    vm_region_basic_info_data_64_t info { };
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &regionStart, &regionSize, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS)
        return false;
    return regionStart <= start && start + data.size() <= regionStart + regionSize && info.max_protection == VM_PROT_READ;
}

TEST(SharedMemoryTransfer, FromIPCDataWrapsDonatedMemory)
{
    auto handle = createDonatedHandle(7);
    ASSERT_TRUE(handle.has_value());

    auto buffer = FragmentedSharedBuffer::fromIPCData(FragmentedSharedBuffer::IPCData { WTF::move(handle) });
    ASSERT_TRUE(buffer.has_value());
    Ref contiguous = (*buffer)->makeContiguous();
    EXPECT_TRUE(isInReadOnlyMapping(contiguous->span()));
    expectTestPattern(contiguous->span(), 7);
}

TEST(SharedMemoryTransfer, FromIPCDataCopiesWhenSenderKeepsMapping)
{
    RefPtr source = SharedMemory::allocate(transferSize);
    ASSERT_NOT_NULL(source);
    fillTestPattern(source->mutableSpan(), 8);
    auto handle = source->createHandle(SharedMemory::Protection::ReadOnly);
    ASSERT_TRUE(handle.has_value());

    auto buffer = FragmentedSharedBuffer::fromIPCData(FragmentedSharedBuffer::IPCData { WTF::move(handle) });
    ASSERT_TRUE(buffer.has_value());
    Ref contiguous = (*buffer)->makeContiguous();
    EXPECT_FALSE(isInReadOnlyMapping(contiguous->span()));
    expectTestPattern(contiguous->span(), 8);
}

TEST(SharedMemoryTransfer, FromIPCDataCopiesVolatileMemory)
{
    auto handle = createDonatedHandle(9, VM_PURGABLE_VOLATILE);
    ASSERT_TRUE(handle.has_value());

    auto buffer = FragmentedSharedBuffer::fromIPCData(FragmentedSharedBuffer::IPCData { WTF::move(handle) });
    ASSERT_TRUE(buffer.has_value());
    Ref contiguous = (*buffer)->makeContiguous();
    EXPECT_FALSE(isInReadOnlyMapping(contiguous->span()));
}
#endif // HAVE(SAFE_MEMORY_TRANSFER)

#if PLATFORM(COCOA)
#define ANY_MEMORY_SOURCE testing::Values(MemorySource::Malloc, MemorySource::SharedMemory, MemorySource::ExplicitMapping)
#define ANY_MEMORY_SIZE testing::Values(1, 2, KB, 100 * KB, 500 * MB, 4 * GB + 1, 20 * GB)
#else
#define ANY_MEMORY_SOURCE testing::Values(MemorySource::Malloc, MemorySource::SharedMemory)
// Overcommit makes allocating the regions over 4 GB succeed, so the physical copy has to commit all of them.
#define ANY_MEMORY_SIZE testing::Values(1, 2, KB, 100 * KB, 500 * MB)
#endif

INSTANTIATE_TEST_SUITE_P(SharedMemoryTest,
    SharedMemoryFromMemoryTest,
    testing::Combine(
        ANY_MEMORY_SIZE,
        testing::Values(0, 1, 444, 4097),
        ANY_MEMORY_SOURCE,
        testing::Values(SharedMemory::Protection::ReadOnly, SharedMemory::Protection::ReadWrite)),
    TestParametersToStringFormatter());

}
