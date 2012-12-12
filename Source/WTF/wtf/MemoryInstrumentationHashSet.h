/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MemoryInstrumentationHashSet_h
#define MemoryInstrumentationHashSet_h

#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentationSequence.h>

namespace WTF {

template<typename ValueArg, typename HashArg, typename TraitsArg>
void reportMemoryUsage(const HashSet<ValueArg, HashArg, TraitsArg>* hashSet, MemoryObjectInfo* memoryObjectInfo)
{
    MemoryClassInfo info(memoryObjectInfo, hashSet);
    info.addPrivateBuffer(sizeof(typename HashTable<ValueArg, ValueArg, IdentityExtractor, HashArg, TraitsArg, TraitsArg>::ValueType) * hashSet->capacity());
    SequenceMemoryInstrumentationTraits<ValueArg>::reportMemoryUsage(hashSet->begin(), hashSet->end(), info);
}

}

#endif // !defined(MemoryInstrumentationHashSet_h)
