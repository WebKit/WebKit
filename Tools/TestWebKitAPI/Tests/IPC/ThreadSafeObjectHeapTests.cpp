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

class TestedObject : public ThreadSafeRefCounted<TestedObject> {
public:
    static RefPtr<TestedObject> create(int value)
    {
        return adoptRef(*new TestedObject(value));
    }
    ~TestedObject()
    {
        s_instances--;
    }
    int value() const { return m_value; }
    static size_t instances() { return s_instances; }
    auto newWriteReference() { return m_referenceTracker.write(); }
    auto newReadReference() { return m_referenceTracker.read(); }
private:
    TestedObject(int value)
        : m_value(value)
    {
        s_instances++;
    }
    const int m_value;
    TestedObjectReferenceTracker m_referenceTracker { TestedObjectIdentifier::generate() };
    static std::atomic<size_t> s_instances;
};

std::atomic<size_t> TestedObject::s_instances;

class ThreadSafeObjectHeapTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
    void TearDown() override
    {
        ASSERT_EQ(TestedObject::instances(), 0u);
    }
};

}

TEST_F(ThreadSafeObjectHeapTest, AddAndGetWorks)
{
    {
        IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, RefPtr<TestedObject>> heap;
        std::optional<TestedObjectReadReference> read1;
        {
            auto obj1 = TestedObject::create(344);
            heap.retire(obj1->newWriteReference(), { obj1 }, std::nullopt);
            read1 = obj1->newReadReference();
        }
        EXPECT_EQ(344, heap.retire(WTFMove(*read1), 0_s)->value());
        EXPECT_EQ(1u, TestedObject::instances());
    }
    EXPECT_EQ(0u, TestedObject::instances());
}

TEST_F(ThreadSafeObjectHeapTest, CompleteReadAfterRemoveWithPendingRead)
{
    IPC::ThreadSafeObjectHeap<TestedObjectIdentifier, RefPtr<TestedObject>> heap;
    std::optional<TestedObjectReadReference> read1;
    std::optional<TestedObjectWriteReference> remove;
    {
        auto obj1 = TestedObject::create(345);
        heap.retire(obj1->newWriteReference(), { obj1 }, std::nullopt);
        read1 = obj1->newReadReference();
        remove = obj1->newWriteReference();
    }
    heap.retireRemove(WTFMove(*remove)); // Non-blocking remove.
    EXPECT_EQ(1u, TestedObject::instances());
    EXPECT_EQ(345, heap.retire(WTFMove(*read1), 0_s)->value());
    EXPECT_EQ(0u, TestedObject::instances());
}

}
