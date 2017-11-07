/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "IsoTLSEntry.h"

namespace bmalloc {

template<typename Func>
void IsoTLSEntry::walkUpToInclusive(IsoTLSEntry* last, const Func& func)
{
    IsoTLSEntry* current = this;
    for (;;) {
        func(current);
        if (current == last)
            return;
        current = current->m_next;
    }
}

template<typename EntryType>
DefaultIsoTLSEntry<EntryType>::DefaultIsoTLSEntry()
    : IsoTLSEntry(alignof(EntryType), sizeof(EntryType))
{
}

template<typename EntryType>
DefaultIsoTLSEntry<EntryType>::~DefaultIsoTLSEntry()
{
}

template<typename EntryType>
void DefaultIsoTLSEntry<EntryType>::move(void* passedSrc, void* dst)
{
    EntryType* src = static_cast<EntryType*>(passedSrc);
    new (dst) EntryType(std::move(*src));
    src->~EntryType();
}

template<typename EntryType>
void DefaultIsoTLSEntry<EntryType>::destruct(void* passedEntry)
{
    EntryType* entry = static_cast<EntryType*>(passedEntry);
    entry->~EntryType();
}

template<typename EntryType>
void DefaultIsoTLSEntry<EntryType>::scavenge(void* passedEntry)
{
    EntryType* entry = static_cast<EntryType*>(passedEntry);
    entry->scavenge();
}

} // namespace bmalloc

