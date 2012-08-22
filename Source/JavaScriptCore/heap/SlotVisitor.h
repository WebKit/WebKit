/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef SlotVisitor_h
#define SlotVisitor_h

#include "CopiedSpace.h"
#include "MarkStack.h"
#include "MarkStackInlineMethods.h"

namespace JSC {

class Heap;
class GCThreadSharedData;

class SlotVisitor : public MarkStack {
    friend class HeapRootVisitor;
public:
    SlotVisitor(GCThreadSharedData&);

    void donate()
    {
        ASSERT(m_isInParallelMode);
        if (Options::numberOfGCMarkers() == 1)
            return;
        
        donateKnownParallel();
    }
    
    void drain();
    
    void donateAndDrain()
    {
        donate();
        drain();
    }
    
    enum SharedDrainMode { SlaveDrain, MasterDrain };
    void drainFromShared(SharedDrainMode);

    void harvestWeakReferences();
    void finalizeUnconditionalFinalizers();

    void startCopying();
    
    // High-level API for copying, appropriate for cases where the object's heap references
    // fall into a contiguous region of the storage chunk and if the object for which you're
    // doing copying does not occur frequently.
    void copyAndAppend(void**, size_t, JSValue*, unsigned);
    
    // Low-level API for copying, appropriate for cases where the object's heap references
    // are discontiguous or if the object occurs frequently enough that you need to focus on
    // performance. Use this with care as it is easy to shoot yourself in the foot.
    bool checkIfShouldCopyAndPinOtherwise(void* oldPtr, size_t);
    void* allocateNewSpace(size_t);
    
    void doneCopying(); 
        
private:
    void* allocateNewSpaceOrPin(void*, size_t);
    void* allocateNewSpaceSlow(size_t);

    void donateKnownParallel();

    CopiedAllocator m_copiedAllocator;
};

inline SlotVisitor::SlotVisitor(GCThreadSharedData& shared)
    : MarkStack(shared)
{
}

} // namespace JSC

#endif // SlotVisitor_h
