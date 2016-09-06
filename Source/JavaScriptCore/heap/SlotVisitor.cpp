/*
 * Copyright (C) 2012, 2015-2016 Apple Inc. All rights reserved.
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
#include "SlotVisitor.h"
#include "SlotVisitorInlines.h"

#include "ConservativeRoots.h"
#include "CopiedBlockInlines.h"
#include "CopiedSpace.h"
#include "CopiedSpaceInlines.h"
#include "HeapCellInlines.h"
#include "HeapProfiler.h"
#include "HeapSnapshotBuilder.h"
#include "JSArray.h"
#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSCInlines.h"
#include "SuperSampler.h"
#include "VM.h"
#include <wtf/Lock.h>

namespace JSC {

#if ENABLE(GC_VALIDATION)
static void validate(JSCell* cell)
{
    RELEASE_ASSERT(cell);

    if (!cell->structure()) {
        dataLogF("cell at %p has a null structure\n" , cell);
        CRASH();
    }

    // Both the cell's structure, and the cell's structure's structure should be the Structure Structure.
    // I hate this sentence.
    if (cell->structure()->structure()->JSCell::classInfo() != cell->structure()->JSCell::classInfo()) {
        const char* parentClassName = 0;
        const char* ourClassName = 0;
        if (cell->structure()->structure() && cell->structure()->structure()->JSCell::classInfo())
            parentClassName = cell->structure()->structure()->JSCell::classInfo()->className;
        if (cell->structure()->JSCell::classInfo())
            ourClassName = cell->structure()->JSCell::classInfo()->className;
        dataLogF("parent structure (%p <%s>) of cell at %p doesn't match cell's structure (%p <%s>)\n",
            cell->structure()->structure(), parentClassName, cell, cell->structure(), ourClassName);
        CRASH();
    }

    // Make sure we can walk the ClassInfo chain
    const ClassInfo* info = cell->classInfo();
    do { } while ((info = info->parentClass));
}
#endif

SlotVisitor::SlotVisitor(Heap& heap)
    : m_stack()
    , m_bytesVisited(0)
    , m_bytesCopied(0)
    , m_visitCount(0)
    , m_isInParallelMode(false)
    , m_version(42)
    , m_heap(heap)
#if !ASSERT_DISABLED
    , m_isCheckingForDefaultMarkViolation(false)
    , m_isDraining(false)
#endif
{
}

SlotVisitor::~SlotVisitor()
{
    clearMarkStack();
}

void SlotVisitor::didStartMarking()
{
    if (heap()->operationInProgress() == FullCollection)
        ASSERT(m_opaqueRoots.isEmpty()); // Should have merged by now.
    else
        reset();

    if (HeapProfiler* heapProfiler = vm().heapProfiler())
        m_heapSnapshotBuilder = heapProfiler->activeSnapshotBuilder();
    
    m_version = heap()->objectSpace().version();
}

void SlotVisitor::reset()
{
    m_bytesVisited = 0;
    m_bytesCopied = 0;
    m_visitCount = 0;
    m_heapSnapshotBuilder = nullptr;
    ASSERT(!m_currentCell);
}

void SlotVisitor::clearMarkStack()
{
    m_stack.clear();
}

void SlotVisitor::append(ConservativeRoots& conservativeRoots)
{
    HeapCell** roots = conservativeRoots.roots();
    size_t size = conservativeRoots.size();
    for (size_t i = 0; i < size; ++i)
        appendJSCellOrAuxiliary(roots[i]);
}

void SlotVisitor::appendJSCellOrAuxiliary(HeapCell* heapCell)
{
    if (!heapCell)
        return;
    
    ASSERT(!m_isCheckingForDefaultMarkViolation);
    
    if (Heap::testAndSetMarked(m_version, heapCell))
        return;
    
    switch (heapCell->cellKind()) {
    case HeapCell::JSCell: {
        JSCell* jsCell = static_cast<JSCell*>(heapCell);
        
        if (!jsCell->structure()) {
            ASSERT_NOT_REACHED();
            return;
        }
        
        jsCell->setCellState(CellState::NewGrey);

        appendToMarkStack(jsCell);
        return;
    }
        
    case HeapCell::Auxiliary: {
        noteLiveAuxiliaryCell(heapCell);
        return;
    } }
}

void SlotVisitor::append(JSValue value)
{
    if (!value || !value.isCell())
        return;

    if (UNLIKELY(m_heapSnapshotBuilder))
        m_heapSnapshotBuilder->appendEdge(m_currentCell, value.asCell());

    setMarkedAndAppendToMarkStack(value.asCell());
}

void SlotVisitor::appendHidden(JSValue value)
{
    if (!value || !value.isCell())
        return;

    setMarkedAndAppendToMarkStack(value.asCell());
}

void SlotVisitor::setMarkedAndAppendToMarkStack(JSCell* cell)
{
    SuperSamplerScope superSamplerScope(false);
    
    ASSERT(!m_isCheckingForDefaultMarkViolation);
    if (!cell)
        return;

#if ENABLE(GC_VALIDATION)
    validate(cell);
#endif
    
    if (cell->isLargeAllocation())
        setMarkedAndAppendToMarkStack(cell->largeAllocation(), cell);
    else
        setMarkedAndAppendToMarkStack(cell->markedBlock(), cell);
}

template<typename ContainerType>
ALWAYS_INLINE void SlotVisitor::setMarkedAndAppendToMarkStack(ContainerType& container, JSCell* cell)
{
    container.flipIfNecessaryConcurrently(m_version);
    
    if (container.testAndSetMarked(cell))
        return;
    
    ASSERT(cell->structure());
    
    // Indicate that the object is grey and that:
    // In case of concurrent GC: it's the first time it is grey in this GC cycle.
    // In case of eden collection: it's a new object that became grey rather than an old remembered object.
    cell->setCellState(CellState::NewGrey);
    
    appendToMarkStack(container, cell);
}

void SlotVisitor::appendToMarkStack(JSCell* cell)
{
    if (cell->isLargeAllocation())
        appendToMarkStack(cell->largeAllocation(), cell);
    else
        appendToMarkStack(cell->markedBlock(), cell);
}

template<typename ContainerType>
ALWAYS_INLINE void SlotVisitor::appendToMarkStack(ContainerType& container, JSCell* cell)
{
    ASSERT(Heap::isMarked(cell));
    ASSERT(!cell->isZapped());
    
    container.noteMarked();
    
    // FIXME: These "just work" because the GC resets these fields before doing anything else. But
    // that won't be the case when we do concurrent GC.
    m_visitCount++;
    m_bytesVisited += container.cellSize();
    
    m_stack.append(cell);

    if (UNLIKELY(m_heapSnapshotBuilder))
        m_heapSnapshotBuilder->appendNode(cell);
}

void SlotVisitor::markAuxiliary(const void* base)
{
    HeapCell* cell = bitwise_cast<HeapCell*>(base);
    
    if (Heap::testAndSetMarked(m_version, cell)) {
        RELEASE_ASSERT(Heap::isMarked(cell));
        return;
    }
    
    noteLiveAuxiliaryCell(cell);
}

void SlotVisitor::noteLiveAuxiliaryCell(HeapCell* cell)
{
    // We get here once per GC under these circumstances:
    //
    // Eden collection: if the cell was allocated since the last collection and is live somehow.
    //
    // Full collection: if the cell is live somehow.
    
    CellContainer container = cell->cellContainer();
    
    container.noteMarked();
    
    m_visitCount++;
    m_bytesVisited += container.cellSize();
}

class SetCurrentCellScope {
public:
    SetCurrentCellScope(SlotVisitor& visitor, const JSCell* cell)
        : m_visitor(visitor)
    {
        ASSERT(!m_visitor.m_currentCell);
        m_visitor.m_currentCell = const_cast<JSCell*>(cell);
    }

    ~SetCurrentCellScope()
    {
        ASSERT(m_visitor.m_currentCell);
        m_visitor.m_currentCell = nullptr;
    }

private:
    SlotVisitor& m_visitor;
};


ALWAYS_INLINE void SlotVisitor::visitChildren(const JSCell* cell)
{
    ASSERT(Heap::isMarked(cell));
    
    SetCurrentCellScope currentCellScope(*this, cell);
    
    m_currentObjectCellStateBeforeVisiting = cell->cellState();
    cell->setCellState(CellState::OldBlack);
    
    if (isJSString(cell)) {
        JSString::visitChildren(const_cast<JSCell*>(cell), *this);
        return;
    }

    if (isJSFinalObject(cell)) {
        JSFinalObject::visitChildren(const_cast<JSCell*>(cell), *this);
        return;
    }

    if (isJSArray(cell)) {
        JSArray::visitChildren(const_cast<JSCell*>(cell), *this);
        return;
    }

    cell->methodTable()->visitChildren(const_cast<JSCell*>(cell), *this);
}

void SlotVisitor::donateKnownParallel()
{
    // NOTE: Because we re-try often, we can afford to be conservative, and
    // assume that donating is not profitable.

    // Avoid locking when a thread reaches a dead end in the object graph.
    if (m_stack.size() < 2)
        return;

    // If there's already some shared work queued up, be conservative and assume
    // that donating more is not profitable.
    if (m_heap.m_sharedMarkStack.size())
        return;

    // If we're contending on the lock, be conservative and assume that another
    // thread is already donating.
    std::unique_lock<Lock> lock(m_heap.m_markingMutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    // Otherwise, assume that a thread will go idle soon, and donate.
    m_stack.donateSomeCellsTo(m_heap.m_sharedMarkStack);

    m_heap.m_markingConditionVariable.notifyAll();
}

void SlotVisitor::drain()
{
    ASSERT(m_isInParallelMode);
   
    while (!m_stack.isEmpty()) {
        m_stack.refill();
        for (unsigned countdown = Options::minimumNumberOfScansBetweenRebalance(); m_stack.canRemoveLast() && countdown--;)
            visitChildren(m_stack.removeLast());
        donateKnownParallel();
    }
    
    mergeOpaqueRootsIfNecessary();
}

void SlotVisitor::drainFromShared(SharedDrainMode sharedDrainMode)
{
    ASSERT(m_isInParallelMode);
    
    ASSERT(Options::numberOfGCMarkers());
    
    {
        std::lock_guard<Lock> lock(m_heap.m_markingMutex);
        m_heap.m_numberOfActiveParallelMarkers++;
    }
    while (true) {
        {
            std::unique_lock<Lock> lock(m_heap.m_markingMutex);
            m_heap.m_numberOfActiveParallelMarkers--;
            m_heap.m_numberOfWaitingParallelMarkers++;

            // How we wait differs depending on drain mode.
            if (sharedDrainMode == MasterDrain) {
                // Wait until either termination is reached, or until there is some work
                // for us to do.
                while (true) {
                    // Did we reach termination?
                    if (!m_heap.m_numberOfActiveParallelMarkers
                        && m_heap.m_sharedMarkStack.isEmpty()) {
                        // Let any sleeping slaves know it's time for them to return;
                        m_heap.m_markingConditionVariable.notifyAll();
                        return;
                    }
                    
                    // Is there work to be done?
                    if (!m_heap.m_sharedMarkStack.isEmpty())
                        break;
                    
                    // Otherwise wait.
                    m_heap.m_markingConditionVariable.wait(lock);
                }
            } else {
                ASSERT(sharedDrainMode == SlaveDrain);
                
                // Did we detect termination? If so, let the master know.
                if (!m_heap.m_numberOfActiveParallelMarkers
                    && m_heap.m_sharedMarkStack.isEmpty())
                    m_heap.m_markingConditionVariable.notifyAll();

                m_heap.m_markingConditionVariable.wait(
                    lock,
                    [this] {
                        return !m_heap.m_sharedMarkStack.isEmpty()
                            || m_heap.m_parallelMarkersShouldExit;
                    });
                
                // Is the current phase done? If so, return from this function.
                if (m_heap.m_parallelMarkersShouldExit)
                    return;
            }

            m_stack.stealSomeCellsFrom(
                m_heap.m_sharedMarkStack, m_heap.m_numberOfWaitingParallelMarkers);
            m_heap.m_numberOfActiveParallelMarkers++;
            m_heap.m_numberOfWaitingParallelMarkers--;
        }
        
        drain();
    }
}

void SlotVisitor::addOpaqueRoot(void* root)
{
    if (Options::numberOfGCMarkers() == 1) {
        // Put directly into the shared HashSet.
        m_heap.m_opaqueRoots.add(root);
        return;
    }
    // Put into the local set, but merge with the shared one every once in
    // a while to make sure that the local sets don't grow too large.
    mergeOpaqueRootsIfProfitable();
    m_opaqueRoots.add(root);
}

bool SlotVisitor::containsOpaqueRoot(void* root) const
{
    ASSERT(!m_isInParallelMode);
    ASSERT(m_opaqueRoots.isEmpty());
    return m_heap.m_opaqueRoots.contains(root);
}

TriState SlotVisitor::containsOpaqueRootTriState(void* root) const
{
    if (m_opaqueRoots.contains(root))
        return TrueTriState;
    std::lock_guard<Lock> lock(m_heap.m_opaqueRootsMutex);
    if (m_heap.m_opaqueRoots.contains(root))
        return TrueTriState;
    return MixedTriState;
}

int SlotVisitor::opaqueRootCount()
{
    ASSERT(!m_isInParallelMode);
    ASSERT(m_opaqueRoots.isEmpty());
    return m_heap.m_opaqueRoots.size();
}

void SlotVisitor::mergeOpaqueRootsIfNecessary()
{
    if (m_opaqueRoots.isEmpty())
        return;
    mergeOpaqueRoots();
}
    
void SlotVisitor::mergeOpaqueRootsIfProfitable()
{
    if (static_cast<unsigned>(m_opaqueRoots.size()) < Options::opaqueRootMergeThreshold())
        return;
    mergeOpaqueRoots();
}
    
void SlotVisitor::donate()
{
    ASSERT(m_isInParallelMode);
    if (Options::numberOfGCMarkers() == 1)
        return;
    
    donateKnownParallel();
}

void SlotVisitor::donateAndDrain()
{
    donate();
    drain();
}

void SlotVisitor::copyLater(JSCell* owner, CopyToken token, void* ptr, size_t bytes)
{
    ASSERT(bytes);
    CopiedBlock* block = CopiedSpace::blockFor(ptr);
    if (block->isOversize()) {
        ASSERT(bytes <= block->size());
        // FIXME: We should be able to shrink the allocation if bytes went below the block size.
        // For now, we just make sure that our accounting of how much memory we are actually using
        // is correct.
        // https://bugs.webkit.org/show_bug.cgi?id=144749
        bytes = block->size();
        m_heap.m_storageSpace.pin(block);
    }

    ASSERT(heap()->m_storageSpace.contains(block));

    LockHolder locker(&block->workListLock());
    // We always report live bytes, except if during an eden collection we see an old object pointing to an
    // old backing store and the old object is being marked because of the remembered set. Note that if we
    // ask the object itself, it will always tell us that it's an old black object - because even during an
    // eden collection we have already indicated that the object is old. That's why we use the
    // SlotVisitor's cache of the object's old state.
    if (heap()->operationInProgress() == FullCollection
        || !block->isOld()
        || m_currentObjectCellStateBeforeVisiting != CellState::OldGrey) {
        m_bytesCopied += bytes;
        block->reportLiveBytes(locker, owner, token, bytes);
    }
}
    
void SlotVisitor::mergeOpaqueRoots()
{
    ASSERT(!m_opaqueRoots.isEmpty()); // Should only be called when opaque roots are non-empty.
    {
        std::lock_guard<Lock> lock(m_heap.m_opaqueRootsMutex);
        for (auto* root : m_opaqueRoots)
            m_heap.m_opaqueRoots.add(root);
    }
    m_opaqueRoots.clear();
}

void SlotVisitor::harvestWeakReferences()
{
    for (WeakReferenceHarvester* current = m_heap.m_weakReferenceHarvesters.head(); current; current = current->next())
        current->visitWeakReferences(*this);
}

void SlotVisitor::finalizeUnconditionalFinalizers()
{
    while (m_heap.m_unconditionalFinalizers.hasNext())
        m_heap.m_unconditionalFinalizers.removeNext()->finalizeUnconditionally();
}

void SlotVisitor::dump(PrintStream&) const
{
    for (const JSCell* cell : markStack())
        dataLog(*cell, "\n");
}

} // namespace JSC
