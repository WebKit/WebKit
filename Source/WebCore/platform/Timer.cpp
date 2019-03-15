/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "Timer.h"

#include "RuntimeApplicationChecks.h"
#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#include <limits.h>
#include <limits>
#include <math.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
#include <wtf/spi/darwin/dyldSPI.h>
#endif

namespace WebCore {

class TimerHeapReference;

// Timers are stored in a heap data structure, used to implement a priority queue.
// This allows us to efficiently determine which timer needs to fire the soonest.
// Then we set a single shared system timer to fire at that time.
//
// When a timer's "next fire time" changes, we need to move it around in the priority queue.
#if !ASSERT_DISABLED
static ThreadTimerHeap& threadGlobalTimerHeap()
{
    return threadGlobalData().threadTimers().timerHeap();
}
#endif

inline ThreadTimerHeapItem::ThreadTimerHeapItem(TimerBase& timer, MonotonicTime time, unsigned insertionOrder)
    : time(time)
    , insertionOrder(insertionOrder)
    , m_threadTimers(threadGlobalData().threadTimers())
    , m_timer(&timer)
{
    ASSERT(m_timer);
}
    
inline RefPtr<ThreadTimerHeapItem> ThreadTimerHeapItem::create(TimerBase& timer, MonotonicTime time, unsigned insertionOrder)
{
    return adoptRef(*new ThreadTimerHeapItem { timer, time, insertionOrder });
}

// ----------------

class TimerHeapPointer {
public:
    TimerHeapPointer(RefPtr<ThreadTimerHeapItem>* pointer)
        : m_pointer(pointer)
    { }

    TimerHeapReference operator*() const;
    RefPtr<ThreadTimerHeapItem>& operator->() const { return *m_pointer; }
private:
    RefPtr<ThreadTimerHeapItem>* m_pointer;
};

class TimerHeapReference {
public:
    TimerHeapReference(RefPtr<ThreadTimerHeapItem>& reference)
        : m_reference(reference)
    { }

    TimerHeapReference(const TimerHeapReference& other)
        : m_reference(other.m_reference)
    { }

    operator RefPtr<ThreadTimerHeapItem>&() const { return m_reference; }
    TimerHeapPointer operator&() const { return &m_reference; }
    TimerHeapReference& operator=(TimerHeapReference&&);
    TimerHeapReference& operator=(RefPtr<ThreadTimerHeapItem>&&);

    void swap(TimerHeapReference& other);

    void updateHeapIndex();

private:
    RefPtr<ThreadTimerHeapItem>& m_reference;

    friend void swap(TimerHeapReference a, TimerHeapReference b);
};

inline TimerHeapReference TimerHeapPointer::operator*() const
{
    return TimerHeapReference { *m_pointer };
}

inline TimerHeapReference& TimerHeapReference::operator=(TimerHeapReference&& other)
{
    m_reference = WTFMove(other.m_reference);
    updateHeapIndex();
    return *this;
}

inline TimerHeapReference& TimerHeapReference::operator=(RefPtr<ThreadTimerHeapItem>&& item)
{
    m_reference = WTFMove(item);
    updateHeapIndex();
    return *this;
}

inline void TimerHeapReference::swap(TimerHeapReference& other)
{
    m_reference.swap(other.m_reference);
    updateHeapIndex();
    other.updateHeapIndex();
}

inline void TimerHeapReference::updateHeapIndex()
{
    auto& heap = m_reference->timerHeap();
    if (&m_reference >= heap.data() && &m_reference < heap.data() + heap.size())
        m_reference->setHeapIndex(&m_reference - heap.data());
}

inline void swap(TimerHeapReference a, TimerHeapReference b)
{
    a.swap(b);
}

// ----------------

// Class to represent iterators in the heap when calling the standard library heap algorithms.
// Uses a custom pointer and reference type that update indices for pointers in the heap.
class TimerHeapIterator : public std::iterator<std::random_access_iterator_tag, RefPtr<ThreadTimerHeapItem>, ptrdiff_t, TimerHeapPointer, TimerHeapReference> {
public:
    explicit TimerHeapIterator(RefPtr<ThreadTimerHeapItem>* pointer) : m_pointer(pointer) { checkConsistency(); }

    TimerHeapIterator& operator++() { checkConsistency(); ++m_pointer; checkConsistency(); return *this; }
    TimerHeapIterator operator++(int) { checkConsistency(1); return TimerHeapIterator(m_pointer++); }

    TimerHeapIterator& operator--() { checkConsistency(); --m_pointer; checkConsistency(); return *this; }
    TimerHeapIterator operator--(int) { checkConsistency(-1); return TimerHeapIterator(m_pointer--); }

    TimerHeapIterator& operator+=(ptrdiff_t i) { checkConsistency(); m_pointer += i; checkConsistency(); return *this; }
    TimerHeapIterator& operator-=(ptrdiff_t i) { checkConsistency(); m_pointer -= i; checkConsistency(); return *this; }

    TimerHeapReference operator*() const { return TimerHeapReference(*m_pointer); }
    TimerHeapReference operator[](ptrdiff_t i) const { return TimerHeapReference(m_pointer[i]); }
    RefPtr<ThreadTimerHeapItem>& operator->() const { return *m_pointer; }

private:
    void checkConsistency(ptrdiff_t offset = 0) const
    {
        ASSERT(m_pointer >= threadGlobalTimerHeap().data());
        ASSERT(m_pointer <= threadGlobalTimerHeap().data() + threadGlobalTimerHeap().size());
        ASSERT_UNUSED(offset, m_pointer + offset >= threadGlobalTimerHeap().data());
        ASSERT_UNUSED(offset, m_pointer + offset <= threadGlobalTimerHeap().data() + threadGlobalTimerHeap().size());
    }

    friend bool operator==(TimerHeapIterator, TimerHeapIterator);
    friend bool operator!=(TimerHeapIterator, TimerHeapIterator);
    friend bool operator<(TimerHeapIterator, TimerHeapIterator);
    friend bool operator>(TimerHeapIterator, TimerHeapIterator);
    friend bool operator<=(TimerHeapIterator, TimerHeapIterator);
    friend bool operator>=(TimerHeapIterator, TimerHeapIterator);
    
    friend TimerHeapIterator operator+(TimerHeapIterator, size_t);
    friend TimerHeapIterator operator+(size_t, TimerHeapIterator);
    
    friend TimerHeapIterator operator-(TimerHeapIterator, size_t);
    friend ptrdiff_t operator-(TimerHeapIterator, TimerHeapIterator);

    RefPtr<ThreadTimerHeapItem>* m_pointer;
};

inline bool operator==(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer == b.m_pointer; }
inline bool operator!=(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer != b.m_pointer; }
inline bool operator<(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer < b.m_pointer; }
inline bool operator>(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer > b.m_pointer; }
inline bool operator<=(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer <= b.m_pointer; }
inline bool operator>=(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer >= b.m_pointer; }

inline TimerHeapIterator operator+(TimerHeapIterator a, size_t b) { return TimerHeapIterator(a.m_pointer + b); }
inline TimerHeapIterator operator+(size_t a, TimerHeapIterator b) { return TimerHeapIterator(a + b.m_pointer); }

inline TimerHeapIterator operator-(TimerHeapIterator a, size_t b) { return TimerHeapIterator(a.m_pointer - b); }
inline ptrdiff_t operator-(TimerHeapIterator a, TimerHeapIterator b) { return a.m_pointer - b.m_pointer; }

// ----------------

class TimerHeapLessThanFunction {
public:
    static bool compare(const TimerBase& a, const RefPtr<ThreadTimerHeapItem>& b)
    {
        return compare(a.m_heapItem->time, a.m_heapItem->insertionOrder, b->time, b->insertionOrder);
    }

    static bool compare(const RefPtr<ThreadTimerHeapItem>& a, const TimerBase& b)
    {
        return compare(a->time, a->insertionOrder, b.m_heapItem->time, b.m_heapItem->insertionOrder);
    }

    bool operator()(const RefPtr<ThreadTimerHeapItem>& a, const RefPtr<ThreadTimerHeapItem>& b) const
    {
        return compare(a->time, a->insertionOrder, b->time, b->insertionOrder);
    }

private:
    static bool compare(MonotonicTime aTime, unsigned aOrder, MonotonicTime bTime, unsigned bOrder)
    {
        // The comparisons below are "backwards" because the heap puts the largest
        // element first and we want the lowest time to be the first one in the heap.
        if (bTime != aTime)
            return bTime < aTime;
        // We need to look at the difference of the insertion orders instead of comparing the two
        // outright in case of overflow.
        unsigned difference = aOrder - bOrder;
        return difference < std::numeric_limits<unsigned>::max() / 2;
    }
};

// ----------------

static bool shouldSuppressThreadSafetyCheck()
{
#if PLATFORM(IOS_FAMILY)
    return WebThreadIsEnabled() || applicationSDKVersion() < DYLD_IOS_VERSION_12_0;
#elif PLATFORM(MAC)
    return !isInWebProcess() && applicationSDKVersion() < DYLD_MACOSX_VERSION_10_14;
#else
    return false;
#endif
}

TimerBase::TimerBase()
{
}

TimerBase::~TimerBase()
{
    ASSERT(canAccessThreadLocalDataForThread(m_thread.get()));
    RELEASE_ASSERT(canAccessThreadLocalDataForThread(m_thread.get()) || shouldSuppressThreadSafetyCheck());
    stop();
    ASSERT(!inHeap());
    if (m_heapItem)
        m_heapItem->clearTimer();
    m_unalignedNextFireTime = MonotonicTime::nan();
}

void TimerBase::start(Seconds nextFireInterval, Seconds repeatInterval)
{
    ASSERT(canAccessThreadLocalDataForThread(m_thread.get()));

    m_repeatInterval = repeatInterval;
    setNextFireTime(MonotonicTime::now() + nextFireInterval);
}

void TimerBase::stop()
{
    ASSERT(canAccessThreadLocalDataForThread(m_thread.get()));

    m_repeatInterval = 0_s;
    setNextFireTime(MonotonicTime { });

    ASSERT(!static_cast<bool>(nextFireTime()));
    ASSERT(m_repeatInterval == 0_s);
    ASSERT(!inHeap());
}

Seconds TimerBase::nextFireInterval() const
{
    ASSERT(isActive());
    ASSERT(m_heapItem);
    MonotonicTime current = MonotonicTime::now();
    auto fireTime = nextFireTime();
    if (fireTime < current)
        return 0_s;
    return fireTime - current;
}

inline void TimerBase::checkHeapIndex() const
{
#if !ASSERT_DISABLED
    ASSERT(m_heapItem);
    auto& heap = m_heapItem->timerHeap();
    ASSERT(&heap == &threadGlobalTimerHeap());
    ASSERT(!heap.isEmpty());
    ASSERT(m_heapItem->isInHeap());
    ASSERT(m_heapItem->heapIndex() < m_heapItem->timerHeap().size());
    ASSERT(heap[m_heapItem->heapIndex()] == m_heapItem);
    for (unsigned i = 0, size = heap.size(); i < size; i++)
        ASSERT(heap[i]->heapIndex() == i);
#endif
}

inline void TimerBase::checkConsistency() const
{
    // Timers should be in the heap if and only if they have a non-zero next fire time.
    ASSERT(inHeap() == static_cast<bool>(nextFireTime()));
    if (inHeap())
        checkHeapIndex();
}

void TimerBase::heapDecreaseKey()
{
    ASSERT(static_cast<bool>(nextFireTime()));
    ASSERT(m_heapItem);
    checkHeapIndex();
    auto* heapData = m_heapItem->timerHeap().data();
    push_heap(TimerHeapIterator(heapData), TimerHeapIterator(heapData + m_heapItem->heapIndex() + 1), TimerHeapLessThanFunction());
    checkHeapIndex();
}

inline void TimerBase::heapDelete()
{
    ASSERT(!static_cast<bool>(nextFireTime()));
    heapPop();
    m_heapItem->timerHeap().removeLast();
    m_heapItem->setNotInHeap();
}

void TimerBase::heapDeleteMin()
{
    ASSERT(!static_cast<bool>(nextFireTime()));
    heapPopMin();
    m_heapItem->timerHeap().removeLast();
    m_heapItem->setNotInHeap();
}

inline void TimerBase::heapIncreaseKey()
{
    ASSERT(static_cast<bool>(nextFireTime()));
    heapPop();
    heapDecreaseKey();
}

inline void TimerBase::heapInsert()
{
    ASSERT(!inHeap());
    ASSERT(m_heapItem);
    auto& heap = m_heapItem->timerHeap();
    heap.append(m_heapItem.copyRef());
    m_heapItem->setHeapIndex(heap.size() - 1);
    heapDecreaseKey();
}

inline void TimerBase::heapPop()
{
    ASSERT(m_heapItem);
    // Temporarily force this timer to have the minimum key so we can pop it.
    MonotonicTime fireTime = m_heapItem->time;
    m_heapItem->time = -MonotonicTime::infinity();
    heapDecreaseKey();
    heapPopMin();
    m_heapItem->time = fireTime;
}

void TimerBase::heapPopMin()
{
    ASSERT(m_heapItem == m_heapItem->timerHeap().first());
    checkHeapIndex();
    auto& heap = m_heapItem->timerHeap();
    auto* heapData = heap.data();
    pop_heap(TimerHeapIterator(heapData), TimerHeapIterator(heapData + heap.size()), TimerHeapLessThanFunction());
    checkHeapIndex();
    ASSERT(m_heapItem == m_heapItem->timerHeap().last());
}

void TimerBase::heapDeleteNullMin(ThreadTimerHeap& heap)
{
    RELEASE_ASSERT(!heap.first()->hasTimer());
    heap.first()->time = -MonotonicTime::infinity();
    auto* heapData = heap.data();
    pop_heap(TimerHeapIterator(heapData), TimerHeapIterator(heapData + heap.size()), TimerHeapLessThanFunction());
    heap.removeLast();
}

static inline bool parentHeapPropertyHolds(const TimerBase* current, const ThreadTimerHeap& heap, unsigned currentIndex)
{
    if (!currentIndex)
        return true;
    unsigned parentIndex = (currentIndex - 1) / 2;
    return TimerHeapLessThanFunction::compare(*current, heap[parentIndex]);
}

static inline bool childHeapPropertyHolds(const TimerBase* current, const ThreadTimerHeap& heap, unsigned childIndex)
{
    if (childIndex >= heap.size())
        return true;
    return TimerHeapLessThanFunction::compare(heap[childIndex], *current);
}

bool TimerBase::hasValidHeapPosition() const
{
    ASSERT(nextFireTime());
    ASSERT(m_heapItem);
    if (!inHeap())
        return false;
    // Check if the heap property still holds with the new fire time. If it does we don't need to do anything.
    // This assumes that the STL heap is a standard binary heap. In an unlikely event it is not, the assertions
    // in updateHeapIfNeeded() will get hit.
    const auto& heap = m_heapItem->timerHeap();
    unsigned heapIndex = m_heapItem->heapIndex();
    if (!parentHeapPropertyHolds(this, heap, heapIndex))
        return false;
    unsigned childIndex1 = 2 * heapIndex + 1;
    unsigned childIndex2 = childIndex1 + 1;
    return childHeapPropertyHolds(this, heap, childIndex1) && childHeapPropertyHolds(this, heap, childIndex2);
}

void TimerBase::updateHeapIfNeeded(MonotonicTime oldTime)
{
    auto fireTime = nextFireTime();
    if (fireTime && hasValidHeapPosition())
        return;

#if !ASSERT_DISABLED
    Optional<unsigned> oldHeapIndex;
    if (m_heapItem->isInHeap())
        oldHeapIndex = m_heapItem->heapIndex();
#endif

    if (!oldTime)
        heapInsert();
    else if (!fireTime)
        heapDelete();
    else if (fireTime < oldTime)
        heapDecreaseKey();
    else
        heapIncreaseKey();

#if !ASSERT_DISABLED
    Optional<unsigned> newHeapIndex;
    if (m_heapItem->isInHeap())
        newHeapIndex = m_heapItem->heapIndex();
    ASSERT(newHeapIndex != oldHeapIndex);
#endif

    ASSERT(!inHeap() || hasValidHeapPosition());
}

void TimerBase::setNextFireTime(MonotonicTime newTime)
{
    ASSERT(canAccessThreadLocalDataForThread(m_thread.get()));
    RELEASE_ASSERT(canAccessThreadLocalDataForThread(m_thread.get()) || shouldSuppressThreadSafetyCheck());
    bool timerHasBeenDeleted = std::isnan(m_unalignedNextFireTime);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!timerHasBeenDeleted);

    if (m_unalignedNextFireTime != newTime) {
        RELEASE_ASSERT(!std::isnan(newTime));
        m_unalignedNextFireTime = newTime;
    }

    // Keep heap valid while changing the next-fire time.
    MonotonicTime oldTime = nextFireTime();
    // Don't realign zero-delay timers.
    if (newTime) {
        if (auto newAlignedTime = alignedFireTime(newTime))
            newTime = newAlignedTime.value();
    }

    if (oldTime != newTime) {
        // FIXME: This should be part of ThreadTimers, or another per-thread structure.
        static std::atomic<unsigned> currentHeapInsertionOrder;
        auto newOrder = currentHeapInsertionOrder++;

        if (!m_heapItem)
            m_heapItem = ThreadTimerHeapItem::create(*this, newTime, 0);
        m_heapItem->time = newTime;
        m_heapItem->insertionOrder = newOrder;

        bool wasFirstTimerInHeap = m_heapItem->isFirstInHeap();

        updateHeapIfNeeded(oldTime);

        bool isFirstTimerInHeap = m_heapItem->isFirstInHeap();

        if (wasFirstTimerInHeap || isFirstTimerInHeap)
            threadGlobalData().threadTimers().updateSharedTimer();
    }

    checkConsistency();
}

void TimerBase::fireTimersInNestedEventLoop()
{
    // Redirect to ThreadTimers.
    threadGlobalData().threadTimers().fireTimersInNestedEventLoop();
}

void TimerBase::didChangeAlignmentInterval()
{
    setNextFireTime(m_unalignedNextFireTime);
}

Seconds TimerBase::nextUnalignedFireInterval() const
{
    ASSERT(isActive());
    auto result = std::max(m_unalignedNextFireTime - MonotonicTime::now(), 0_s);
    RELEASE_ASSERT(std::isfinite(result));
    return result;
}

} // namespace WebCore

