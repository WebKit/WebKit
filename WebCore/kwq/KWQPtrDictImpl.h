/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef KWQ_PTRDICT_IMPL_H
#define KWQ_PTRDICT_IMPL_H

#include "KWQDef.h"

#include <stddef.h>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

class KWQPtrDictPrivate;

class KWQPtrDictImpl
{
 public:
#if __APPLE__
    KWQPtrDictImpl(int size, void (*deleteFunc)(void *), const CFDictionaryKeyCallBacks *cfdkcb = NULL);
#else
    KWQPtrDictImpl(int size, void (*deleteFunc)(void *));
#endif

    KWQPtrDictImpl(const KWQPtrDictImpl &pdi);
    ~KWQPtrDictImpl();
    
    void clear(bool deleteItems);
    uint count() const;

    void *take(void *key);
    void insert(void *key, const void *value);
    bool remove(void *key, bool deleteItem);
    void *find(void *key) const;

    KWQPtrDictImpl &assign(const KWQPtrDictImpl &pdi, bool deleteItems);
    
 private:
    void swap(KWQPtrDictImpl &di);

    KWQPtrDictPrivate *d;
    
    friend class KWQPtrDictIteratorImpl;
};


class KWQPtrDictIteratorPrivate;
    
class KWQPtrDictIteratorImpl {
 public:
    KWQPtrDictIteratorImpl(const KWQPtrDictImpl &pdi);
    ~KWQPtrDictIteratorImpl();

    uint count() const;
    void *current() const;
    void *currentKey() const;
    void *toFirst();

    void *operator++();
    
 private:
    KWQPtrDictIteratorPrivate *d;
};

#endif
