/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Canvas2DLayerManager.h"

#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using testing::InSequence;
using testing::Return;
using testing::Test;


class FakeCanvas2DLayerBridge : public Canvas2DLayerBridge {
public:
    FakeCanvas2DLayerBridge() 
        : Canvas2DLayerBridge(GraphicsContext3DPrivate::createGraphicsContextFromWebContext(adoptPtr(new WebKit::FakeWebGraphicsContext3D)), IntSize(1, 1), Deferred, 0)
        , m_freeableBytes(0)
        , m_freeMemoryIfPossibleCount(0)
        , m_flushCount(0)
    {
    }

    void fakeFreeableBytes(size_t size)
    {
        m_freeableBytes = size;
    }

    virtual size_t freeMemoryIfPossible(size_t size) OVERRIDE
    {
        m_freeMemoryIfPossibleCount++;
        size_t bytesFreed = size < m_freeableBytes ? size : m_freeableBytes;
        m_freeableBytes -= bytesFreed;
        Canvas2DLayerManager::get().layerAllocatedStorageChanged(this, -((intptr_t)bytesFreed));
        m_bytesAllocated -= bytesFreed;
        return bytesFreed;
    }

    virtual void flush() OVERRIDE
    {
        m_flushCount++;
    }

public:
    size_t m_freeableBytes;
    int m_freeMemoryIfPossibleCount;
    int m_flushCount;
};

class Canvas2DLayerManagerTest : public Test {
protected:
    void storageAllocationTrackingTest()
    {
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 10);
        {
            FakeCanvas2DLayerBridge layer1;
            EXPECT_EQ((size_t)0, manager.m_bytesAllocated);
            layer1.storageAllocatedForRecordingChanged(1);
            EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
            // Test allocation increase
            layer1.storageAllocatedForRecordingChanged(2);
            EXPECT_EQ((size_t)2, manager.m_bytesAllocated);
            // Test allocation decrease
            layer1.storageAllocatedForRecordingChanged(1);
            EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
            {
                FakeCanvas2DLayerBridge layer2;
                EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
                // verify multi-layer allocation tracking
                layer2.storageAllocatedForRecordingChanged(2);
                EXPECT_EQ((size_t)3, manager.m_bytesAllocated);
            }
            // Verify tracking after destruction
            EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
        }
    }

    void evictionTest()
    {
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 5);
        FakeCanvas2DLayerBridge layer;
        layer.fakeFreeableBytes(10);
        layer.storageAllocatedForRecordingChanged(8); // under the max
        EXPECT_EQ(0, layer.m_freeMemoryIfPossibleCount);
        layer.storageAllocatedForRecordingChanged(12); // over the max
        EXPECT_EQ(1, layer.m_freeMemoryIfPossibleCount);
        EXPECT_EQ((size_t)3, layer.m_freeableBytes);
        EXPECT_EQ(0, layer.m_flushCount); // eviction succeeded without triggering a flush
        EXPECT_EQ((size_t)5, layer.bytesAllocated());
    }

    void flushEvictionTest()
    {
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 5);
        FakeCanvas2DLayerBridge layer;
        layer.fakeFreeableBytes(1); // Not enough freeable bytes, will cause aggressive eviction by flushing
        layer.storageAllocatedForRecordingChanged(8); // under the max
        EXPECT_EQ(0, layer.m_freeMemoryIfPossibleCount);
        layer.storageAllocatedForRecordingChanged(12); // over the max
        EXPECT_EQ(2, layer.m_freeMemoryIfPossibleCount); // Two tries, one before flush, one after flush
        EXPECT_EQ((size_t)0, layer.m_freeableBytes);
        EXPECT_EQ(1, layer.m_flushCount); // flush was attempted
        EXPECT_EQ((size_t)11, layer.bytesAllocated()); // flush drops the layer from manager's tracking list
        EXPECT_FALSE(manager.isInList(&layer));
    }
};

namespace {

TEST_F(Canvas2DLayerManagerTest, testStorageAllocationTracking)
{
    storageAllocationTrackingTest();
}

TEST_F(Canvas2DLayerManagerTest, testEviction)
{
    evictionTest();
}

TEST_F(Canvas2DLayerManagerTest, testFlushEviction)
{
    flushEvictionTest();
}

} // namespace

