/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ThreadSafeObjectHeap.h"
#include <atomic>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

namespace {

enum TestedObjectIdentifierType { };
using TestedObjectIdentifier = AtomicObjectIdentifier<TestedObjectIdentifierType>;
using TestedObjectReadReference = IPC::ObjectIdentifierReadReference<TestedObjectIdentifier>;
using TestedObjectWriteReference = IPC::ObjectIdentifierWriteReference<TestedObjectIdentifier>;
using TestedObjectReference = IPC::ObjectIdentifierReference<TestedObjectIdentifier>;
using TestedObjectReferenceTracker = IPC::ObjectIdentifierReferenceTracker<TestedObjectIdentifier>;

// TestObject instances are the instances that the receiver stores in its object heap.
class TestedObject : public ThreadSafeRefCounted<TestedObject> {
public:
    static Ref<TestedObject> create(int value)
    {
        return adoptRef(*new TestedObject(value));
    }
    ~TestedObject()
    {
        s_instances--;
    }
    int value() const { return m_value; }
    static size_t instances() { return s_instances; }

private:
    TestedObject(int value)
        : m_value(value)
    {
        s_instances++;
    }
    const int m_value;
    static std::atomic<size_t> s_instances;
};

std::atomic<size_t> TestedObject::s_instances;

class ThreadSafeObjectHeapTest : public testing::Test {
protected:
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
    void TearDown() override
    {
        ASSERT_EQ(TestedObject::instances(), 0u);
    }

    // These are for the reference tracking that happens in the sender.
    TestedObjectReferenceTracker m_referenceTracker { TestedObjectIdentifier::generate() };
    auto newAddReference() { return m_referenceTracker.add(); }
    auto newWriteReference() { return m_referenceTracker.write(); }
    auto newReadReference() { return m_referenceTracker.read(); }
};

}

TEST_F(ThreadSafeObjectHeapTest, AddAndGetWorks)
{
    {
        IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
        EXPECT_TRUE(heap.add(newAddReference(), TestedObject::create(344)));
        auto read1 = newReadReference();

        EXPECT_EQ(344, heap.get(read1, 0_s)->value());
        EXPECT_EQ(1u, TestedObject::instances());
        EXPECT_EQ(1u, heap.sizeForTesting());
    }
    EXPECT_EQ(0u, TestedObject::instances());
}

TEST_F(ThreadSafeObjectHeapTest, CompleteReadAfterRemoveWithPendingRead)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    EXPECT_TRUE(heap.add(newAddReference(), TestedObject::create(345)));
    auto read1 = newReadReference();
    auto remove = newWriteReference();

    heap.remove(WTFMove(remove)); // Non-blocking remove.
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(345, heap.read(WTFMove(read1), 0_s)->value());
    EXPECT_EQ(0u, heap.sizeForTesting());
    EXPECT_EQ(0u, TestedObject::instances());
}

TEST_F(ThreadSafeObjectHeapTest, ReadWriteWorks)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    EXPECT_TRUE(heap.add(newAddReference(), TestedObject::create(1)));
    auto read1 = newReadReference();
    auto write1 = newWriteReference();

    EXPECT_EQ(1, heap.read(WTFMove(read1), 0_s)->value());
    EXPECT_EQ(1, heap.take(WTFMove(write1), 0_s)->value());
    EXPECT_EQ(0u, TestedObject::instances());
}

TEST_F(ThreadSafeObjectHeapTest, RemoveWorks)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    EXPECT_TRUE(heap.add(newAddReference(), TestedObject::create(1)));
    EXPECT_EQ(1, heap.read(newReadReference(), 0_s)->value());
    auto write1 = newWriteReference();

    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_TRUE(heap.remove(WTFMove(write1)));
    EXPECT_EQ(0u, heap.sizeForTesting());
    EXPECT_EQ(0u, TestedObject::instances());
}

TEST_F(ThreadSafeObjectHeapTest, WriteBeforeRetireEarlierReadTimesOut)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    EXPECT_TRUE(heap.add(newAddReference(), TestedObject::create(1)));
    auto read1 = newReadReference();
    auto write1 = newWriteReference();

    // Test that take times out.
    EXPECT_EQ(nullptr, heap.take(WTFMove(write1), 0_s));

    // Ensure that write works after read.
    EXPECT_EQ(1, heap.read(WTFMove(read1), 0_s)->value());
    EXPECT_EQ(1, heap.take(WTFMove(write1), 0_s)->value());
    EXPECT_EQ(0u, TestedObject::instances());
    EXPECT_EQ(0u, heap.sizeForTesting());
}

TEST_F(ThreadSafeObjectHeapTest, ReadBeforeRetireEarlierWriteTimesOut)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    heap.add(newAddReference(), TestedObject::create(1));
    auto write1 = newWriteReference();
    auto read1 = newReadReference();
    auto newReference = write1.retiredReference();

    EXPECT_EQ(nullptr, heap.read(WTFMove(read1), 0_s));
    EXPECT_EQ(1, heap.take(WTFMove(write1), 0_s)->value());
    EXPECT_TRUE(heap.add(newReference, TestedObject::create(2)));
    EXPECT_EQ(2, heap.get(read1, 0_s)->value());
}

TEST_F(ThreadSafeObjectHeapTest, RemoveBeforeAdd)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    auto add1 = newAddReference();
    auto write1 = newWriteReference();

    EXPECT_TRUE(heap.remove(WTFMove(write1)));
    EXPECT_EQ(1u, heap.sizeForTesting());
    EXPECT_TRUE(heap.add(add1, TestedObject::create(1)));
    EXPECT_EQ(0u, TestedObject::instances());
    EXPECT_EQ(0u, heap.sizeForTesting());
}

TEST_F(ThreadSafeObjectHeapTest, RemoveBeforeAddAndReads)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    auto add1 = newAddReference();
    auto read1 = newReadReference();
    auto read2 = newReadReference();
    auto read3 = newReadReference();
    auto write1 = newWriteReference();

    EXPECT_TRUE(heap.remove(WTFMove(write1)));
    EXPECT_EQ(1u, heap.sizeForTesting());
    EXPECT_TRUE(heap.add(add1, TestedObject::create(1)));
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(1, heap.read(WTFMove(read1), 0_s)->value());
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(1, heap.read(WTFMove(read2), 0_s)->value());
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(1, heap.read(WTFMove(read3), 0_s)->value());
    EXPECT_EQ(0u, TestedObject::instances());
    EXPECT_EQ(0u, heap.sizeForTesting());
}

TEST_F(ThreadSafeObjectHeapTest, RemoveBeforeReads)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    auto add1 = newAddReference();
    auto read1 = newReadReference();
    auto read2 = newReadReference();
    auto read3 = newReadReference();
    auto write1 = newWriteReference();

    EXPECT_TRUE(heap.add(add1, TestedObject::create(1)));
    EXPECT_EQ(1u, heap.sizeForTesting());
    EXPECT_TRUE(heap.remove(WTFMove(write1)));
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(1, heap.read(WTFMove(read1), 0_s)->value());
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(1, heap.read(WTFMove(read2), 0_s)->value());
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(1, heap.read(WTFMove(read3), 0_s)->value());
    EXPECT_EQ(0u, TestedObject::instances());
    EXPECT_EQ(0u, heap.sizeForTesting());
}

TEST_F(ThreadSafeObjectHeapTest, RemoveBeforeWritesAndReads)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    auto add1 = newAddReference();
    auto write1 = newWriteReference();
    auto add2 = newAddReference();
    auto read2 = newReadReference();
    auto write2 = newWriteReference();
    auto add3 = newAddReference();
    auto read3 = newReadReference();
    auto write3 = newWriteReference();

    EXPECT_TRUE(heap.remove(WTFMove(write3)));
    EXPECT_TRUE(heap.remove(WTFMove(write2)));
    EXPECT_TRUE(heap.remove(WTFMove(write1)));
    EXPECT_EQ(3u, heap.sizeForTesting());

    EXPECT_EQ(0u, TestedObject::instances());
    EXPECT_TRUE(heap.add(add1, TestedObject::create(1)));
    EXPECT_TRUE(heap.add(add2, TestedObject::create(2)));
    EXPECT_TRUE(heap.add(add3, TestedObject::create(3)));
    EXPECT_EQ(2u, TestedObject::instances());
    EXPECT_EQ(2u, heap.sizeForTesting());
    EXPECT_EQ(2, heap.read(WTFMove(read2), 0_s)->value());
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(3, heap.read(WTFMove(read3), 0_s)->value());
    EXPECT_EQ(0u, TestedObject::instances());
}

TEST_F(ThreadSafeObjectHeapTest, InvalidRemoveTwice)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    TestedObjectWriteReference write1 { TestedObjectReference::generateForAdd(), 0 };
    TestedObjectWriteReference write2 { write1.reference(), 0 };
    TestedObjectWriteReference write3 { write1.reference(), 0 };

    EXPECT_TRUE(heap.remove(WTFMove(write1)));
    EXPECT_FALSE(heap.remove(WTFMove(write2)));
    EXPECT_FALSE(heap.take(WTFMove(write3), 0_s));
}

TEST_F(ThreadSafeObjectHeapTest, InvalidRemoveTooFewReads)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    auto add1 = newAddReference();
    auto read1 = newReadReference();
    EXPECT_TRUE(heap.add(add1, TestedObject::create(1)));
    EXPECT_EQ(1, heap.read(WTFMove(read1), 0_s)->value());

    // Already read once, but trying to remove with 0 pending reads.
    TestedObjectWriteReference write1 { add1, 0 };
    TestedObjectWriteReference write2 { add1, 0 };

    EXPECT_FALSE(heap.remove(WTFMove(write1)));
    EXPECT_FALSE(heap.take(WTFMove(write2), 0_s));
}

TEST_F(ThreadSafeObjectHeapTest, InvalidAddTwice)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, Ref<TestedObject>> heap;
    auto add1 = newAddReference();
    EXPECT_TRUE(heap.add(add1, TestedObject::create(1)));
    EXPECT_FALSE(heap.add(add1, TestedObject::create(1)));
}

}
