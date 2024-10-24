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

#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#include <limits>
#include <math.h>
#include <wtf/MainThread.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThread.h"
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TimerBase);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Timer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(DeferrableOneShotTimer);

class TimerHeapReference;

// Timers are stored in a heap data structure, used to implement a priority queue.
// This allows us to efficiently determine which timer needs to fire the soonest.
// Then we set a single shared system timer to fire at that time.
//
// When a timer's "next fire time" changes, we need to move it around in the priority queue.
#if ASSERT_ENABLED
static ThreadTimerHeap& threadGlobalTimerHeap()
{
    return threadGlobalData().threadTimers().timerHeap();
}
#endif

WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL(ThreadTimerHeapItem);

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
class TimerHeapIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = RefPtr<ThreadTimerHeapItem>;
    using difference_type = ptrdiff_t;
    using pointer = TimerHeapPointer;
    using reference = TimerHeapReference;

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

    friend bool operator==(TimerHeapIterator, TimerHeapIterator) = default;
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
        return compare(a.m_heapItemWithBitfields.pointer()->time, a.m_heapItemWithBitfields.pointer()->insertionOrder, b->time, b->insertionOrder);
    }

    static bool compare(const RefPtr<ThreadTimerHeapItem>& a, const TimerBase& b)
    {
        return compare(a->time, a->insertionOrder, b.m_heapItemWithBitfields.pointer()->time, b.m_heapItemWithBitfields.pointer()->insertionOrder);
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
    return WebThreadIsEnabled() || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::TimerThreadSafetyChecks);
#elif PLATFORM(MAC)
    return !isInWebProcess() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::TimerThreadSafetyChecks);
#else
    return false;
#endif
}

struct SameSizeAsTimer {
    virtual ~SameSizeAsTimer() { }

    WeakPtr<TimerAlignment> timerAlignment;
    double times[2];
    void* pointers[2];
#if CPU(ADDRESS32)
    uint8_t bitfields;
#endif
    void* pointer;
};

static_assert(sizeof(Timer) == sizeof(SameSizeAsTimer), "Timer should stay small");

struct SameSizeAsDeferrableOneShotTimer : public SameSizeAsTimer {
    double delay;
};

static_assert(sizeof(DeferrableOneShotTimer) == sizeof(SameSizeAsDeferrableOneShotTimer), "DeferrableOneShotTimer should stay small");

TimerBase::TimerBase()
{
#if USE(WEB_THREAD)
    RELEASE_ASSERT(WebThreadIsLockedOrDisabledInMainOrWebThread());
#endif
}

TimerBase::~TimerBase()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_thread));
    RELEASE_ASSERT(canCurrentThreadAccessThreadLocalData(m_thread) || shouldSuppressThreadSafetyCheck());
    stop();
    ASSERT(!inHeap());
    if (auto* item = m_heapItemWithBitfields.pointer())
        item->clearTimer();
    m_unalignedNextFireTime = MonotonicTime::nan();
}

void TimerBase::start(Seconds nextFireInterval, Seconds repeatInterval)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_thread));

    m_repeatInterval = repeatInterval;
    setNextFireTime(MonotonicTime::now() + nextFireInterval);
}

void TimerBase::stopSlowCase()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_thread));

    m_repeatInterval = 0_s;
    setNextFireTime(MonotonicTime { });

    ASSERT(!static_cast<bool>(nextFireTime()));
    ASSERT(m_repeatInterval == 0_s);
    ASSERT(!inHeap());
}

Seconds TimerBase::nextFireInterval() const
{
    ASSERT(isActive());
    ASSERT(m_heapItemWithBitfields.pointer());
    MonotonicTime current = MonotonicTime::now();
    auto fireTime = nextFireTime();
    if (fireTime < current)
        return 0_s;
    return fireTime - current;
}

inline void TimerBase::checkHeapIndex() const
{
#if ASSERT_ENABLED
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    auto& heap = item->timerHeap();
    ASSERT(&heap == &threadGlobalTimerHeap());
    ASSERT(!heap.isEmpty());
    ASSERT(item->isInHeap());
    ASSERT(item->heapIndex() < heap.size());
    ASSERT(heap[item->heapIndex()] == item);
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
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    checkHeapIndex();
    auto* heapData = item->timerHeap().data();
    std::push_heap(TimerHeapIterator(heapData), TimerHeapIterator(heapData + item->heapIndex() + 1), TimerHeapLessThanFunction());
    checkHeapIndex();
}

inline void TimerBase::heapDelete()
{
    ASSERT(!static_cast<bool>(nextFireTime()));
    heapPop();
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    item->timerHeap().removeLast();
    item->setNotInHeap();
}

void TimerBase::heapDeleteMin()
{
    ASSERT(!static_cast<bool>(nextFireTime()));
    heapPopMin();
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    item->timerHeap().removeLast();
    item->setNotInHeap();
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
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    auto& heap = item->timerHeap();
    heap.append(item);
    item->setHeapIndex(heap.size() - 1);
    heapDecreaseKey();
}

inline void TimerBase::heapPop()
{
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    // Temporarily force this timer to have the minimum key so we can pop it.
    MonotonicTime fireTime = item->time;
    item->time = -MonotonicTime::infinity();
    heapDecreaseKey();
    heapPopMin();
    item->time = fireTime;
}

void TimerBase::heapPopMin()
{
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    ASSERT(item == item->timerHeap().first());
    checkHeapIndex();
    auto& heap = item->timerHeap();
    auto* heapData = heap.data();
    std::pop_heap(TimerHeapIterator(heapData), TimerHeapIterator(heapData + heap.size()), TimerHeapLessThanFunction());
    checkHeapIndex();
    ASSERT(item == item->timerHeap().last());
}

void TimerBase::heapDeleteNullMin(ThreadTimerHeap& heap)
{
    RELEASE_ASSERT(!heap.first()->hasTimer());
    heap.first()->time = -MonotonicTime::infinity();
    auto* heapData = heap.data();
    std::pop_heap(TimerHeapIterator(heapData), TimerHeapIterator(heapData + heap.size()), TimerHeapLessThanFunction());
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
    RefPtr item = m_heapItemWithBitfields.pointer();
    ASSERT(item);
    if (!inHeap())
        return false;
    // Check if the heap property still holds with the new fire time. If it does we don't need to do anything.
    // This assumes that the STL heap is a standard binary heap. In an unlikely event it is not, the assertions
    // in updateHeapIfNeeded() will get hit.
    const auto& heap = item->timerHeap();
    unsigned heapIndex = item->heapIndex();
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

#if ASSERT_ENABLED
    std::optional<unsigned> oldHeapIndex;
    auto* item = m_heapItemWithBitfields.pointer();
    if (item->isInHeap())
        oldHeapIndex = item->heapIndex();
#endif

    if (!oldTime)
        heapInsert();
    else if (!fireTime)
        heapDelete();
    else if (fireTime < oldTime)
        heapDecreaseKey();
    else
        heapIncreaseKey();

#if ASSERT_ENABLED
    std::optional<unsigned> newHeapIndex;
    if (item->isInHeap())
        newHeapIndex = item->heapIndex();
    ASSERT(newHeapIndex != oldHeapIndex);
#endif

    ASSERT(!inHeap() || hasValidHeapPosition());
}

void TimerBase::setNextFireTime(MonotonicTime newTime)
{
#if USE(WEB_THREAD)
    RELEASE_ASSERT(WebThreadIsLockedOrDisabledInMainOrWebThread());
#endif
    ASSERT(canCurrentThreadAccessThreadLocalData(m_thread));
    RELEASE_ASSERT(canCurrentThreadAccessThreadLocalData(m_thread) || shouldSuppressThreadSafetyCheck());
    bool timerHasBeenDeleted = m_unalignedNextFireTime.isNaN();
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!timerHasBeenDeleted);

    if (m_unalignedNextFireTime != newTime) {
        RELEASE_ASSERT(!newTime.isNaN());
        m_unalignedNextFireTime = newTime;
    }

    // Keep heap valid while changing the next-fire time.
    MonotonicTime oldTime = nextFireTime();
    // Don't realign zero-delay timers.
    if (auto* alignment = m_alignment.get(); newTime && alignment) {
        if (auto newAlignedTime = alignment->alignedFireTime(hasReachedMaxNestingLevel(), newTime))
            newTime = newAlignedTime.value();
    }

    if (oldTime != newTime) {
        auto newOrder = threadGlobalData().threadTimers().nextHeapInsertionCount();

        RefPtr item = m_heapItemWithBitfields.pointer();
        if (!item) {
            m_heapItemWithBitfields.setPointer(ThreadTimerHeapItem::create(*this, newTime, 0));
            item = m_heapItemWithBitfields.pointer();
        }
        item->time = newTime;
        item->insertionOrder = newOrder;

        bool wasFirstTimerInHeap = item->isFirstInHeap();

        updateHeapIfNeeded(oldTime);

        bool isFirstTimerInHeap = item->isFirstInHeap();

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
    RELEASE_ASSERT(result.isFinite());
    return result;
}

} // namespace WebCore

