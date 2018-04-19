/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "IsoSubspace.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace JSC {

// This is an appropriate way to stash IsoSubspaces for rarely-used classes or classes that are mostly
// sure to be main-thread-only. But if a class typically gets instantiated from multiple threads at
// once, then this is not great, because concurrent allocations will probably contend on this thing's
// lock.
class IsoSubspacePerVM {
public:
    struct SubspaceParameters {
        SubspaceParameters() { }
        
        SubspaceParameters(CString name, HeapCellType* heapCellType, size_t size)
            : name(WTFMove(name))
            , heapCellType(heapCellType)
            , size(size)
        {
        }
        
        CString name;
        HeapCellType* heapCellType { nullptr };
        size_t size { 0 };
    };
    
    JS_EXPORT_PRIVATE IsoSubspacePerVM(Function<SubspaceParameters(VM&)>);
    JS_EXPORT_PRIVATE ~IsoSubspacePerVM();
    
    JS_EXPORT_PRIVATE IsoSubspace& forVM(VM&);

private:
    class AutoremovingIsoSubspace;
    friend class AutoremovingIsoSubspace;

    Lock m_lock;
    HashMap<VM*, IsoSubspace*> m_subspacePerVM;
    Function<SubspaceParameters(VM&)> m_subspaceParameters;
};

#define ISO_SUBSPACE_PARAMETERS(heapCellType, type) ::JSC::IsoSubspacePerVM::SubspaceParameters("Isolated " #type " Space", (heapCellType), sizeof(type))

} // namespace JSC

