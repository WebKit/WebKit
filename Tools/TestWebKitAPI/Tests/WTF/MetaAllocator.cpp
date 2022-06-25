/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <stdarg.h>
#include <wtf/MetaAllocator.h>
#include <wtf/Vector.h>

#if OS(WINDOWS)
#undef small
#endif

namespace TestWebKitAPI {

using namespace WTF;

class MetaAllocatorTest: public testing::Test {
public:
    enum SanityCheckMode { RunSanityCheck, DontRunSanityCheck };
    
    enum HeapGrowthMode { DontGrowHeap, ForTestDemandAllocCoalesce, ForTestDemandAllocDontCoalesce };
    
    HeapGrowthMode currentHeapGrowthMode;
    size_t allowAllocatePages;
    size_t requestedNumPages;

    class SimpleTestAllocator: public MetaAllocator {
    public:
        SimpleTestAllocator(MetaAllocatorTest* parent)
            : MetaAllocator(m_lock, 32)
            , m_parent(parent)
        {
            addFreshFreeSpace(reinterpret_cast<void*>(basePage * pageSize()), defaultPagesInHeap * pageSize());
        }
        
        virtual ~SimpleTestAllocator()
        {
            EXPECT_TRUE(!m_parent->allocatorDestroyed);
            m_parent->allocatorDestroyed = true;
        }
        
        virtual FreeSpacePtr allocateNewSpace(size_t& numPages)
        {
            switch (m_parent->currentHeapGrowthMode) {
            case DontGrowHeap:
                return nullptr;
                
            case ForTestDemandAllocCoalesce:
            case ForTestDemandAllocDontCoalesce: {
                EXPECT_TRUE(m_parent->allowAllocatePages);
                EXPECT_TRUE(m_parent->allowAllocatePages >= numPages);
                m_parent->requestedNumPages = numPages;
                numPages = m_parent->allowAllocatePages;
                
                unsigned offset;
                if (m_parent->currentHeapGrowthMode == ForTestDemandAllocCoalesce)
                    offset = 0;
                else
                    offset = 1;
                
                void* result = reinterpret_cast<void*>((basePage + defaultPagesInHeap + offset) * pageSize());
                
                m_parent->allowAllocatePages = 0;
                m_parent->currentHeapGrowthMode = DontGrowHeap;
                
                for (size_t counter = 0; counter < numPages + offset; ++counter) {
                    m_parent->pageMap->append(false);
                    for (unsigned byteCounter = 0; byteCounter < pageSize(); ++byteCounter)
                        m_parent->memoryMap->append(false);
                }
                
                m_parent->additionalPagesInHeap += numPages;
        
                return FreeSpacePtr::makeFromRawPointer(result);
            }
                
            default:
                CRASH();
                return nullptr;
            }
        }
        
        virtual void notifyNeedPage(void* page, size_t count)
        {
            // the page should be both free and unmapped.
            for (size_t i = 0; i < count; ++i)
                EXPECT_TRUE(!m_parent->pageState(reinterpret_cast<uintptr_t>(page) / pageSize() + i));
            for (uintptr_t address = reinterpret_cast<uintptr_t>(page); address < reinterpret_cast<uintptr_t>(page) + pageSize() * count; ++address)
                EXPECT_TRUE(!m_parent->byteState(reinterpret_cast<void*>(address)));
            for (size_t i = 0; i < count; ++i)
                m_parent->pageState(reinterpret_cast<uintptr_t>(page) / pageSize() + i) = true;
        }
        
        virtual void notifyPageIsFree(void* page, size_t count)
        {
            // the page should be free of objects at this point, but it should still
            // be mapped.
            for (size_t i = 0; i < count; ++i)
                EXPECT_TRUE(m_parent->pageState(reinterpret_cast<uintptr_t>(page) / pageSize() + i));
            for (uintptr_t address = reinterpret_cast<uintptr_t>(page); address < reinterpret_cast<uintptr_t>(page) + pageSize() * count; ++address)
                EXPECT_TRUE(!m_parent->byteState(reinterpret_cast<void*>(address)));
            for (size_t i = 0; i < count; ++i)
                m_parent->pageState(reinterpret_cast<uintptr_t>(page) / pageSize() + i) = false;
        }
        
    private:
        Lock m_lock;
        MetaAllocatorTest* m_parent;
    };

    static const unsigned basePage = 1;
    static const unsigned defaultPagesInHeap = 100;
    
    unsigned additionalPagesInHeap;
    
    Vector<bool, 0>* memoryMap;
    Vector<bool, 0>* pageMap;
    bool allocatorDestroyed;
    
    SimpleTestAllocator* allocator;
    
    virtual void SetUp()
    {
        memoryMap = new Vector<bool, 0>();
        pageMap = new Vector<bool, 0>();
        
        for (unsigned page = basePage; page < basePage + defaultPagesInHeap; ++page) {
            pageMap->append(false);
            for (unsigned byteInPage = 0; byteInPage < pageSize(); ++byteInPage)
                memoryMap->append(false);
        }

        allocatorDestroyed = false;
        
        currentHeapGrowthMode = DontGrowHeap;
        allowAllocatePages = 0;
        additionalPagesInHeap = 0;
        requestedNumPages = 0;
        
        allocator = new SimpleTestAllocator(this);
    }
    
    virtual void TearDown()
    {
        EXPECT_TRUE(currentHeapGrowthMode == DontGrowHeap);
        EXPECT_EQ(allowAllocatePages, static_cast<size_t>(0));
        EXPECT_EQ(requestedNumPages, static_cast<size_t>(0));
        
        // memory should be free.
        for (unsigned page = basePage; page < basePage + defaultPagesInHeap; ++page) {
            EXPECT_TRUE(!pageState(page));
            for (unsigned byteInPage = 0; byteInPage < pageSize(); ++byteInPage)
                EXPECT_TRUE(!byteState(page * pageSize() + byteInPage));
        }
        
        // NOTE: this automatically tests that the allocator did not leak
        // memory, so long as these tests are running with !defined(NDEBUG).
        // See MetaAllocator::m_mallocBalance.
        delete allocator;
        
        EXPECT_TRUE(allocatorDestroyed);
        
        delete memoryMap;
        delete pageMap;
    }
    
    MetaAllocatorHandle* allocate(size_t sizeInBytes, SanityCheckMode sanityCheckMode = RunSanityCheck)
    {
        MetaAllocatorHandle* handle = allocator->allocate(sizeInBytes).leakRef();
        EXPECT_TRUE(handle);
        EXPECT_EQ(handle->sizeInBytes(), sizeInBytes);
        
        uintptr_t startByte = handle->start().untaggedPtr<uintptr_t>();
        uintptr_t endByte = handle->end().untaggedPtr<uintptr_t>();
        for (uintptr_t currentByte = startByte; currentByte < endByte; ++currentByte) {
            EXPECT_TRUE(!byteState(currentByte));
            byteState(currentByte) = true;
            EXPECT_TRUE(pageState(currentByte / pageSize()));
        }
        
        if (sanityCheckMode == RunSanityCheck)
            sanityCheck();
        
        return handle;
    }
    
    void free(MetaAllocatorHandle* handle, SanityCheckMode sanityCheckMode = RunSanityCheck)
    {
        EXPECT_TRUE(handle);
        
        notifyFree(handle->start().untaggedPtr(), handle->sizeInBytes());
        handle->deref();
        
        if (sanityCheckMode == RunSanityCheck)
            sanityCheck();
    }
    
    void notifyFree(void* start, size_t sizeInBytes)
    {
        uintptr_t startByte = reinterpret_cast<uintptr_t>(start);
        uintptr_t endByte = startByte + sizeInBytes;
        for (uintptr_t currentByte = startByte; currentByte < endByte; ++currentByte) {
            EXPECT_TRUE(byteState(currentByte));
            byteState(currentByte) = false;
        }
    }
    
    void sanityCheck()
    {
#ifndef NDEBUG
        EXPECT_EQ(allocator->bytesReserved() - allocator->bytesAllocated(), allocator->debugFreeSpaceSize());
#endif
        EXPECT_EQ(allocator->bytesReserved(), (defaultPagesInHeap + additionalPagesInHeap) * pageSize());
        EXPECT_EQ(allocator->bytesAllocated(), bytesAllocated());
        EXPECT_EQ(allocator->bytesCommitted(), bytesCommitted());
    }
    
    void confirm(MetaAllocatorHandle* handle)
    {
        uintptr_t startByte = handle->start().untaggedPtr<uintptr_t>();
        confirm(startByte, startByte + handle->sizeInBytes(), true);
    }
    
    void confirmHighWatermark(MetaAllocatorHandle* handle)
    {
        confirm(handle->end().untaggedPtr<uintptr_t>(), (basePage + defaultPagesInHeap) * pageSize(), false);
    }
                
    void confirm(uintptr_t startByte, uintptr_t endByte, bool value)
    {
        for (uintptr_t currentByte = startByte; currentByte < endByte; ++currentByte) {
            EXPECT_EQ(byteState(currentByte), value);
            if (value)
                EXPECT_TRUE(pageState(currentByte / pageSize()));
        }
        if (!value) {
            uintptr_t firstFreePage = (startByte + pageSize() - 1) / pageSize();
            uintptr_t lastFreePage = (endByte - pageSize()) / pageSize();
            for (uintptr_t currentPage = firstFreePage; currentPage <= lastFreePage; ++currentPage)
                EXPECT_TRUE(!pageState(currentPage));
        }
    }
    
    size_t bytesAllocated()
    {
        size_t result = 0;
        for (unsigned index = 0; index < memoryMap->size(); ++index) {
            if (memoryMap->at(index))
                result++;
        }
        return result;
    }
    
    size_t bytesCommitted()
    {
        size_t result = 0;
        for (unsigned index = 0; index < pageMap->size(); ++index) {
            if (pageMap->at(index))
                result++;
        }
        return result * pageSize();
    }
    
    bool& byteState(void* address)
    {
        return byteState(reinterpret_cast<uintptr_t>(address));
    }
    
    bool& byteState(uintptr_t address)
    {
        uintptr_t byteIndex = address - basePage * pageSize();
        return memoryMap->at(byteIndex);
    }
    
    bool& pageState(uintptr_t page)
    {
        uintptr_t pageIndex = page - basePage;
        return pageMap->at(pageIndex);
    }

    // Test helpers

    void testOneAlloc(size_t size)
    {
        // Tests the most basic behavior: allocate one thing and free it. Also
        // verifies that the state of pages is correct.
        
        MetaAllocatorHandle* handle = allocate(size);
        EXPECT_EQ(handle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        EXPECT_EQ(handle->sizeInBytes(), size);
        EXPECT_TRUE(pageState(basePage));
        
        confirm(handle);
        confirmHighWatermark(handle);
        
        free(handle);
    }
    
    void testRepeatAllocFree(size_t firstSize, ...)
    {
        // Tests right-coalescing by repeatedly allocating and freeing. The idea
        // is that if you allocate something and then free it, then the heap should
        // look identical to what it was before the allocation due to a right-coalesce
        // of the freed chunk and the already-free memory, and so subsequent
        // allocations should behave the same as the first one.
        
        MetaAllocatorHandle* handle = allocate(firstSize);
        EXPECT_EQ(handle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        EXPECT_EQ(handle->sizeInBytes(), firstSize);
        
        confirm(handle);
        confirmHighWatermark(handle);
        
        free(handle);
        
        va_list argList;
        va_start(argList, firstSize);
        while (size_t sizeInBytes = va_arg(argList, int)) {
            handle = allocate(sizeInBytes);
            EXPECT_EQ(handle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
            EXPECT_EQ(handle->sizeInBytes(), sizeInBytes);
            
            confirm(handle);
            confirmHighWatermark(handle);
            
            free(handle);
        }
        va_end(argList);
    }
    
    void testSimpleFullCoalesce(size_t firstSize, size_t secondSize, size_t thirdSize)
    {
        // Allocates something of size firstSize, then something of size secondSize, and then
        // frees the first allocation, and then the second, and then attempts to allocate the
        // third, asserting that it allocated at the base address of the heap.
        
        // Note that this test may cause right-allocation, which will cause the test to fail.
        // Thus the correct way of running this test is to ensure that secondSize is
        // picked in such a way that it never straddles a page.
        
        MetaAllocatorHandle* firstHandle = allocate(firstSize);
        EXPECT_EQ(firstHandle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        EXPECT_EQ(firstHandle->sizeInBytes(), firstSize);
        
        confirm(firstHandle);
        confirmHighWatermark(firstHandle);

        MetaAllocatorHandle* secondHandle = allocate(secondSize);
        EXPECT_EQ(secondHandle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize() + firstSize));
        EXPECT_EQ(secondHandle->sizeInBytes(), secondSize);
        
        confirm(firstHandle);
        confirm(secondHandle);
        confirmHighWatermark(secondHandle);
        
        free(firstHandle);
        
        confirm(secondHandle);
        confirmHighWatermark(secondHandle);
        
        free(secondHandle);
        
        confirm(basePage * pageSize(), (basePage + defaultPagesInHeap) * pageSize(), false);
        
        MetaAllocatorHandle* thirdHandle = allocate(thirdSize);
        EXPECT_EQ(thirdHandle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        EXPECT_EQ(thirdHandle->sizeInBytes(), thirdSize);
        
        confirm(thirdHandle);
        confirmHighWatermark(thirdHandle);
        
        free(thirdHandle);
    }
    
    enum class TestFIFOAllocMode { FillAtEnd, EagerFill };
    void testFIFOAlloc(TestFIFOAllocMode mode, ...)
    {
        // This will test the simple case of no-coalesce (freeing the left-most
        // chunk in memory when the chunk to the right of it is allocated) and
        // fully exercise left-coalescing and full-coalescing. In EagerFill
        // mode, this also tests perfect-fit allocation and no-coalescing free.
        
        size_t totalSize = 0;
        
        Vector<MetaAllocatorHandle*, 0> handles;
        
        va_list argList;
        va_start(argList, mode);
        while (size_t sizeInBytes = va_arg(argList, int)) {
            MetaAllocatorHandle* handle = allocate(sizeInBytes);
            EXPECT_EQ(handle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize() + totalSize));
            EXPECT_EQ(handle->sizeInBytes(), sizeInBytes);
            
            confirm(handle);
            confirmHighWatermark(handle);
            
            handles.append(handle);
            totalSize += sizeInBytes;
        }
        va_end(argList);
        
        for (unsigned index = 0; index < handles.size(); ++index)
            confirm(handles.at(index));
        
        size_t sizeSoFar = 0;
        for (unsigned index = 0; index < handles.size(); ++index) {
            sizeSoFar += handles.at(index)->sizeInBytes();
            free(handles.at(index));
            if (mode == TestFIFOAllocMode::EagerFill) {
                MetaAllocatorHandle* handle = allocate(sizeSoFar);
                EXPECT_EQ(handle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
                EXPECT_EQ(handle->sizeInBytes(), sizeSoFar);
                
                confirm(basePage * pageSize(), basePage * pageSize() + totalSize, true);
                if (index < handles.size() - 1)
                    confirmHighWatermark(handles.last());
                else
                    confirmHighWatermark(handle);
                
                free(handle);
                
                confirm(basePage * pageSize(), basePage * pageSize() + sizeSoFar, false);
            }
        }
        
        ASSERT(sizeSoFar == totalSize);
        
        confirm(basePage * pageSize(), (basePage + defaultPagesInHeap) * pageSize(), false);
        
        if (mode == TestFIFOAllocMode::FillAtEnd) {
            MetaAllocatorHandle* finalHandle = allocate(totalSize);
            EXPECT_EQ(finalHandle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
            EXPECT_EQ(finalHandle->sizeInBytes(), totalSize);
            
            confirm(finalHandle);
            confirmHighWatermark(finalHandle);
            
            free(finalHandle);
        }
    }
    
    void testFillHeap(size_t sizeInBytes, size_t numAllocations)
    {
        Vector<MetaAllocatorHandle*, 0> handles;
        
        for (size_t index = 0; index < numAllocations; ++index)
            handles.append(allocate(sizeInBytes, DontRunSanityCheck));
        
        sanityCheck();
        
        EXPECT_TRUE(!allocator->allocate(sizeInBytes));
        
        for (size_t index = 0; index < numAllocations; ++index)
            free(handles.at(index), DontRunSanityCheck);
        
        sanityCheck();
    }
    
    void testRightAllocation(size_t firstLeftSize, size_t firstRightSize, size_t secondLeftSize, size_t secondRightSize)
    {
        MetaAllocatorHandle* firstLeft = allocate(firstLeftSize);
        EXPECT_EQ(firstLeft->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        
        MetaAllocatorHandle* firstRight = allocate(firstRightSize);
        EXPECT_EQ(firstRight->end().untaggedPtr(), reinterpret_cast<void*>((basePage + defaultPagesInHeap) * pageSize()));
        
        MetaAllocatorHandle* secondLeft = allocate(secondLeftSize);
        EXPECT_EQ(secondLeft->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize() + firstLeft->sizeInBytes()));
        
        MetaAllocatorHandle* secondRight = allocate(secondRightSize);
        EXPECT_EQ(secondRight->end().untaggedPtr(), reinterpret_cast<void*>((basePage + defaultPagesInHeap) * pageSize() - firstRight->sizeInBytes()));
        
        free(firstLeft);
        free(firstRight);
        free(secondLeft);
        free(secondRight);
        
        MetaAllocatorHandle* final = allocate(defaultPagesInHeap * pageSize());
        EXPECT_EQ(final->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        
        free(final);
    }
    
    void testBestFit(size_t firstSize, size_t step, unsigned numSlots, SanityCheckMode sanityCheckMode)
    {
        Vector<MetaAllocatorHandle*, 0> handlesToFree;
        Vector<MetaAllocatorHandle*, 0> handles;
        Vector<void*, 0> locations;
        
        size_t size = firstSize;
        for (unsigned index = 0; index < numSlots; ++index) {
            MetaAllocatorHandle* toFree = allocate(size, sanityCheckMode);
            if (!handles.isEmpty()) {
                while (toFree->start().untaggedPtr() != handles.last()->end().untaggedPtr()) {
                    handlesToFree.append(toFree);
                    toFree = allocate(size, sanityCheckMode);
                }
            }

            MetaAllocatorHandle* fragger = allocate(32, sanityCheckMode);
            EXPECT_EQ(fragger->start().untaggedPtr(), toFree->end().untaggedPtr());
            
            locations.append(toFree->start().untaggedPtr());

            handlesToFree.append(toFree);
            handles.append(fragger);
            
            size += step;
        }
        
        ASSERT(locations.size() == numSlots);
        
        for (unsigned index = 0; index < handlesToFree.size(); ++index)
            free(handlesToFree.at(index), sanityCheckMode);
        
        size = firstSize;
        for (unsigned index = 0; index < numSlots; ++index) {
            MetaAllocatorHandle* bestFit = allocate(size - 32, sanityCheckMode);
            
            EXPECT_TRUE(bestFit->start().untaggedPtr() == locations.at(index)
                || bestFit->end().untaggedPtr() == reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(locations.at(index)) + size));
            
            MetaAllocatorHandle* small = allocate(32, sanityCheckMode);
            if (bestFit->start().untaggedPtr() == locations.at(index))
                EXPECT_EQ(small->start().untaggedPtr(), bestFit->end().untaggedPtr());
            else
                EXPECT_EQ(small->end().untaggedPtr(), bestFit->start().untaggedPtr());
            
            free(bestFit, sanityCheckMode);
            free(small, sanityCheckMode);
            
            size += step;
        }
        
        for (unsigned index = 0; index < numSlots; ++index)
            free(handles.at(index), sanityCheckMode);
        
        MetaAllocatorHandle* final = allocate(defaultPagesInHeap * pageSize(), sanityCheckMode);
        EXPECT_EQ(final->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        
        free(final, sanityCheckMode);
    }
    
    void testShrink(size_t firstSize, size_t secondSize)
    {
        // Allocate the thing that will be shrunk
        MetaAllocatorHandle* handle = allocate(firstSize);
        
        // Shrink it, and make sure that our state reflects the shrinkage.
        notifyFree(reinterpret_cast<void*>(handle->start().untaggedPtr<uintptr_t>() + secondSize), firstSize - secondSize);
        
        handle->shrink(secondSize);
        EXPECT_EQ(handle->sizeInBytes(), secondSize);
        
        sanityCheck();
        
        // Assert that the heap is not empty.
        EXPECT_TRUE(!allocator->allocate(defaultPagesInHeap * pageSize()));
        
        // Allocate the remainder of the heap.
        MetaAllocatorHandle* remainder = allocate(defaultPagesInHeap * pageSize() - secondSize);
        EXPECT_EQ(remainder->start().untaggedPtr(), handle->end().untaggedPtr());
        
        free(remainder);
        free(handle);
        
        // Assert that the heap is empty and finish up.
        MetaAllocatorHandle* final = allocate(defaultPagesInHeap * pageSize());
        EXPECT_EQ(final->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        
        free(final);
    }
    
    void testDemandAllocCoalesce(size_t firstSize, size_t numPages, size_t secondSize)
    {
        EXPECT_TRUE(!allocator->allocate((defaultPagesInHeap + numPages) * pageSize()));
        
        MetaAllocatorHandle* firstHandle = allocate(firstSize);
        
        EXPECT_TRUE(!allocator->allocate(secondSize));
        EXPECT_TRUE(!allocator->allocate((defaultPagesInHeap + numPages) * pageSize()));
        
        currentHeapGrowthMode = ForTestDemandAllocCoalesce;
        allowAllocatePages = numPages;
        
        MetaAllocatorHandle* secondHandle = allocate(secondSize);
        
        EXPECT_TRUE(currentHeapGrowthMode == DontGrowHeap);
        EXPECT_EQ(allowAllocatePages, static_cast<size_t>(0));
        EXPECT_EQ(requestedNumPages, (secondSize + pageSize() - 1) / pageSize());
        EXPECT_EQ(secondHandle->start().untaggedPtr(), reinterpret_cast<void*>((basePage + defaultPagesInHeap) * pageSize()));
        
        requestedNumPages = 0;
        
        free(firstHandle);
        free(secondHandle);
        
        free(allocate((defaultPagesInHeap + numPages) * pageSize()));
    }
    
    void testDemandAllocDontCoalesce(size_t firstSize, size_t numPages, size_t secondSize)
    {
        free(allocate(firstSize));
        free(allocate(defaultPagesInHeap * pageSize()));
        EXPECT_TRUE(!allocator->allocate((defaultPagesInHeap + numPages) * pageSize()));
        
        MetaAllocatorHandle* firstHandle = allocate(firstSize);
        
        EXPECT_TRUE(!allocator->allocate(secondSize));
        EXPECT_TRUE(!allocator->allocate((defaultPagesInHeap + numPages) * pageSize()));
        
        currentHeapGrowthMode = ForTestDemandAllocDontCoalesce;
        allowAllocatePages = numPages;
        
        MetaAllocatorHandle* secondHandle = allocate(secondSize);
        
        EXPECT_TRUE(currentHeapGrowthMode == DontGrowHeap);
        EXPECT_EQ(allowAllocatePages, static_cast<size_t>(0));
        EXPECT_EQ(requestedNumPages, (secondSize + pageSize() - 1) / pageSize());
        EXPECT_EQ(secondHandle->start().untaggedPtr(), reinterpret_cast<void*>((basePage + defaultPagesInHeap + 1) * pageSize()));
        
        requestedNumPages = 0;
        
        EXPECT_TRUE(!allocator->allocate((defaultPagesInHeap + numPages) * pageSize()));

        free(firstHandle);
        free(secondHandle);
        
        EXPECT_TRUE(!allocator->allocate((defaultPagesInHeap + numPages) * pageSize()));
        
        firstHandle = allocate(firstSize);
        secondHandle = allocate(secondSize);
        EXPECT_EQ(firstHandle->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
        EXPECT_EQ(secondHandle->start().untaggedPtr(), reinterpret_cast<void*>((basePage + defaultPagesInHeap + 1) * pageSize()));
        free(firstHandle);
        free(secondHandle);
    }
};

TEST_F(MetaAllocatorTest, Empty)
{
    // Tests that creating and destroying an allocator works.
}

TEST_F(MetaAllocatorTest, AllocZero)    
{
    // Tests that allocating a zero-length block returns 0 and
    // does not change anything in memory.
    
    ASSERT(!allocator->allocate(0));
    
    MetaAllocatorHandle* final = allocate(defaultPagesInHeap * pageSize());
    EXPECT_EQ(final->start().untaggedPtr(), reinterpret_cast<void*>(basePage * pageSize()));
    free(final);
}

TEST_F(MetaAllocatorTest, OneAlloc32)
{
    testOneAlloc(32);
}

TEST_F(MetaAllocatorTest, OneAlloc64)
{
    testOneAlloc(64);
}

TEST_F(MetaAllocatorTest, OneAllocTwoPages)
{
    testOneAlloc(pageSize() * 2);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32Twice)
{
    testRepeatAllocFree(32, 32, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32Then64)
{
    testRepeatAllocFree(32, 64, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree64Then32)
{
    testRepeatAllocFree(64, 32, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32TwiceThen64)
{
    testRepeatAllocFree(32, 32, 64, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32Then64Twice)
{
    testRepeatAllocFree(32, 64, 64, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree64Then32Then64)
{
    testRepeatAllocFree(64, 32, 64, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32Thrice)
{
    testRepeatAllocFree(32, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32Then64Then32)
{
    testRepeatAllocFree(32, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree64Then32Twice)
{
    testRepeatAllocFree(64, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFreeTwoPagesThen32)
{
    testRepeatAllocFree(static_cast<int>(pageSize() * 2), 32, 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFree32ThenTwoPages)
{
    testRepeatAllocFree(32, static_cast<int>(pageSize() * 2), 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFreePageThenTwoPages)
{
    testRepeatAllocFree(static_cast<int>(pageSize()), static_cast<int>(pageSize() * 2), 0);
}

TEST_F(MetaAllocatorTest, RepeatAllocFreeTwoPagesThenPage)
{
    testRepeatAllocFree(static_cast<int>(pageSize() * 2), static_cast<int>(pageSize()), 0);
}

TEST_F(MetaAllocatorTest, SimpleFullCoalesce32Plus32Then128)
{
    testSimpleFullCoalesce(32, 32, 128);
}

TEST_F(MetaAllocatorTest, SimpleFullCoalesce32Plus64Then128)
{
    testSimpleFullCoalesce(32, 64, 128);
}

TEST_F(MetaAllocatorTest, SimpleFullCoalesce64Plus32Then128)
{
    testSimpleFullCoalesce(64, 32, 128);
}

TEST_F(MetaAllocatorTest, SimpleFullCoalesce32PlusPageLess32ThenPage)
{
    testSimpleFullCoalesce(32, pageSize() - 32, pageSize());
}

TEST_F(MetaAllocatorTest, SimpleFullCoalesce32PlusPageLess32ThenTwoPages)
{
    testSimpleFullCoalesce(32, pageSize() - 32, pageSize() * 2);
}

TEST_F(MetaAllocatorTest, SimpleFullCoalescePagePlus32ThenTwoPages)
{
    testSimpleFullCoalesce(pageSize(), 32, pageSize() * 2);
}

TEST_F(MetaAllocatorTest, SimpleFullCoalescePagePlusPageThenTwoPages)
{
    testSimpleFullCoalesce(pageSize(), pageSize(), pageSize() * 2);
}

TEST_F(MetaAllocatorTest, FIFOAllocFillAtEnd32Twice)
{
    testFIFOAlloc(TestFIFOAllocMode::FillAtEnd, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocFillAtEnd32Thrice)
{
    testFIFOAlloc(TestFIFOAllocMode::FillAtEnd, 32, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocFillAtEnd32FourTimes)
{
    testFIFOAlloc(TestFIFOAllocMode::FillAtEnd, 32, 32, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocFillAtEndPageLess32Then32ThenPageLess64Then64)
{
    testFIFOAlloc(TestFIFOAllocMode::FillAtEnd, static_cast<int>(pageSize() - 32), 32, static_cast<int>(pageSize() - 64), 64, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocEagerFill32Twice)
{
    testFIFOAlloc(TestFIFOAllocMode::EagerFill, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocEagerFill32Thrice)
{
    testFIFOAlloc(TestFIFOAllocMode::EagerFill, 32, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocEagerFill32FourTimes)
{
    testFIFOAlloc(TestFIFOAllocMode::EagerFill, 32, 32, 32, 32, 0);
}

TEST_F(MetaAllocatorTest, FIFOAllocEagerFillPageLess32Then32ThenPageLess64Then64)
{
    testFIFOAlloc(TestFIFOAllocMode::EagerFill, static_cast<int>(pageSize() - 32), 32, static_cast<int>(pageSize() - 64), 64, 0);
}

TEST_F(MetaAllocatorTest, FillHeap32)
{
    testFillHeap(32, defaultPagesInHeap * pageSize() / 32);
}

TEST_F(MetaAllocatorTest, FillHeapPage)
{
    testFillHeap(pageSize(), defaultPagesInHeap);
}

TEST_F(MetaAllocatorTest, FillHeapTwoPages)
{
    testFillHeap(pageSize() * 2, defaultPagesInHeap / 2);
}

TEST_F(MetaAllocatorTest, RightAllocation32ThenPageThen32ThenPage)
{
    testRightAllocation(32, pageSize(), 32, pageSize());
}

TEST_F(MetaAllocatorTest, RightAllocationQuarterPageThenPageThenQuarterPageThenPage)
{
    testRightAllocation(pageSize() / 4, pageSize(), pageSize() / 4, pageSize());
}

TEST_F(MetaAllocatorTest, BestFit64Plus64Thrice)
{
    testBestFit(64, 64, 3, RunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit64Plus64TenTimes)
{
    testBestFit(64, 64, 10, DontRunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit64Plus64HundredTimes)
{
    testBestFit(64, 64, 100, DontRunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit96Plus64Thrice)
{
    testBestFit(96, 64, 3, RunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit96Plus64TenTimes)
{
    testBestFit(96, 64, 10, DontRunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit96Plus64HundredTimes)
{
    testBestFit(96, 64, 100, DontRunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit96Plus96Thrice)
{
    testBestFit(96, 96, 3, RunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit96Plus96TenTimes)
{
    testBestFit(96, 96, 10, DontRunSanityCheck);
}

TEST_F(MetaAllocatorTest, BestFit96Plus96EightyTimes)
{
    testBestFit(96, 96, 80, DontRunSanityCheck);
}

TEST_F(MetaAllocatorTest, Shrink64To32)
{
    testShrink(64, 32);
}

TEST_F(MetaAllocatorTest, ShrinkPageTo32)
{
    testShrink(pageSize(), 32);
}

TEST_F(MetaAllocatorTest, ShrinkPageToPageLess32)
{
    testShrink(pageSize(), pageSize() - 32);
}

TEST_F(MetaAllocatorTest, ShrinkTwoPagesTo32)
{
    testShrink(pageSize() * 2, 32);
}

TEST_F(MetaAllocatorTest, ShrinkTwoPagesToPagePlus32)
{
    testShrink(pageSize() * 2, pageSize() + 32);
}

TEST_F(MetaAllocatorTest, ShrinkTwoPagesToPage)
{
    testShrink(pageSize() * 2, pageSize());
}

TEST_F(MetaAllocatorTest, ShrinkTwoPagesToPageLess32)
{
    testShrink(pageSize() * 2, pageSize() - 32);
}

TEST_F(MetaAllocatorTest, ShrinkTwoPagesToTwoPagesLess32)
{
    testShrink(pageSize() * 2, pageSize() * 2 - 32);
}

TEST_F(MetaAllocatorTest, DemandAllocCoalescePageThenDoubleHeap)
{
    testDemandAllocCoalesce(pageSize(), defaultPagesInHeap, defaultPagesInHeap * pageSize());
}

TEST_F(MetaAllocatorTest, DemandAllocCoalescePageThenTripleHeap)
{
    testDemandAllocCoalesce(pageSize(), defaultPagesInHeap * 2, defaultPagesInHeap * pageSize());
}

TEST_F(MetaAllocatorTest, DemandAllocDontCoalescePageThenDoubleHeap)
{
    testDemandAllocDontCoalesce(pageSize(), defaultPagesInHeap, defaultPagesInHeap * pageSize());
}

} // namespace TestWebKitAPI

#if USE(POINTER_PROFILING)

namespace WTF {

const char* tagForPtr(const void*) { return "<unknown>"; }
const char* ptrTagName(PtrTag) { return "<unknown>"; }

} // namespace WTF

#endif // USE(POINTER_PROFILING)
