/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "ArrayBufferNeuteringWatchpointSet.h"

#include "JSCInlines.h"

namespace JSC {

const ClassInfo ArrayBufferNeuteringWatchpointSet::s_info = {
    "ArrayBufferNeuteringWatchpointSet", nullptr, nullptr, nullptr,
    CREATE_METHOD_TABLE(ArrayBufferNeuteringWatchpointSet)
};

ArrayBufferNeuteringWatchpointSet::ArrayBufferNeuteringWatchpointSet(VM& vm)
    : Base(vm, vm.arrayBufferNeuteringWatchpointStructure.get())
    , m_set(adoptRef(*new WatchpointSet(IsWatched)))
{
}

void ArrayBufferNeuteringWatchpointSet::destroy(JSCell* cell)
{
    static_cast<ArrayBufferNeuteringWatchpointSet*>(cell)->ArrayBufferNeuteringWatchpointSet::~ArrayBufferNeuteringWatchpointSet();
}

ArrayBufferNeuteringWatchpointSet* ArrayBufferNeuteringWatchpointSet::create(VM& vm)
{
    ArrayBufferNeuteringWatchpointSet* result = new
        (NotNull, allocateCell<ArrayBufferNeuteringWatchpointSet>(vm.heap))
        ArrayBufferNeuteringWatchpointSet(vm);
    result->finishCreation(vm);
    return result;
}

Structure* ArrayBufferNeuteringWatchpointSet::createStructure(VM& vm)
{
    return Structure::create(vm, 0, jsNull(), TypeInfo(CellType, StructureFlags), info());
}

void ArrayBufferNeuteringWatchpointSet::fireAll()
{
    m_set->fireAll(vm(), "Array buffer was neutered");
}

} // namespace JSC

