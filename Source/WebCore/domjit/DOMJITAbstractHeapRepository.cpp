/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "DOMJITAbstractHeapRepository.h"

#include <domjit/DOMJITAbstractHeap.h>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(JIT)

namespace WebCore { namespace DOMJIT {

static const bool verbose = false;

AbstractHeapRepository::AbstractHeapRepository()
{
    JSC::DOMJIT::AbstractHeap DOMHeap("DOM");
#define DOMJIT_DEFINE_HEAP(name, parent) JSC::DOMJIT::AbstractHeap name##Heap(#name);
    DOMJIT_ABSTRACT_HEAP_LIST(DOMJIT_DEFINE_HEAP)
#undef DOMJIT_DEFINE_HEAP

#define DOMJIT_INITIALIZE_HEAP(name, parent) name##Heap.setParent(&parent##Heap);
    DOMJIT_ABSTRACT_HEAP_LIST(DOMJIT_INITIALIZE_HEAP)
#undef DOMJIT_INITIALIZE_HEAP

    DOMHeap.compute(0);

#define DOMJIT_INITIALIZE_MEMBER(name, parent) name = name##Heap.range();
    DOMJIT_ABSTRACT_HEAP_LIST(DOMJIT_INITIALIZE_MEMBER)
#undef DOMJIT_INITIALIZE_MEMBER

    if (verbose) {
        dataLog("DOMJIT Heap Repository:\n");
        DOMHeap.deepDump(WTF::dataFile());
    }
}

const AbstractHeapRepository& AbstractHeapRepository::shared()
{
    static NeverDestroyed<AbstractHeapRepository> repository;
    return repository.get();
}

} }

#endif
