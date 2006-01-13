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

#include "config.h"
#include "KWQPtrDictImpl.h"

#include <new>

typedef void (* DeleteFunction) (void *);

class KWQPtrDictPrivate
{
public:
    KWQPtrDictPrivate(int size, DeleteFunction, const CFDictionaryKeyCallBacks *cfdkcb);
    KWQPtrDictPrivate(const KWQPtrDictPrivate &dp);
    ~KWQPtrDictPrivate();
    
    CFMutableDictionaryRef cfdict;
    DeleteFunction del;
    KWQPtrDictIteratorPrivate *iterators;
};

class KWQPtrDictIteratorPrivate
{
public:
    KWQPtrDictIteratorPrivate(KWQPtrDictPrivate *);
    ~KWQPtrDictIteratorPrivate();
    
    void remove(void *key);
    void dictDestroyed();
    
    uint count;
    uint pos;
    void **keys;
    void **values;
    KWQPtrDictPrivate *dict;
    KWQPtrDictIteratorPrivate *next;
    KWQPtrDictIteratorPrivate *prev;
};


KWQPtrDictPrivate::KWQPtrDictPrivate(int size, DeleteFunction deleteFunc, const CFDictionaryKeyCallBacks *cfdkcb) :
    cfdict(CFDictionaryCreateMutable(NULL, 0, cfdkcb, NULL)),
    del(deleteFunc),
    iterators(0)
{
}

KWQPtrDictPrivate::KWQPtrDictPrivate(const KWQPtrDictPrivate &dp) :     
    cfdict(CFDictionaryCreateMutableCopy(NULL, 0, dp.cfdict)),
    del(dp.del),
    iterators(0)
{
}

KWQPtrDictPrivate::~KWQPtrDictPrivate()
{
    for (KWQPtrDictIteratorPrivate *it = iterators; it; it = it->next) {
        it->dictDestroyed();
    }
    CFRelease(cfdict);
}

KWQPtrDictImpl::KWQPtrDictImpl(int size, DeleteFunction deleteFunc, const CFDictionaryKeyCallBacks *cfdkcb) :
    d(new KWQPtrDictPrivate(size, deleteFunc, cfdkcb))
{
}

KWQPtrDictImpl::KWQPtrDictImpl(const KWQPtrDictImpl &di) :
    d(new KWQPtrDictPrivate(*di.d))
{
}


KWQPtrDictImpl::~KWQPtrDictImpl()
{
    delete d;
}

static void invokeDeleteFuncOnValue (const void *key, const void *value, void *context)
{
    DeleteFunction *deleteFunc = (DeleteFunction *)context;
    (*deleteFunc)((void *)value);
}

void KWQPtrDictImpl::clear(bool deleteItems)
{
    if (deleteItems) {
        DeleteFunction deleteFunc = d->del;
        CFDictionaryApplyFunction(d->cfdict, invokeDeleteFuncOnValue, &deleteFunc);
    }

    CFDictionaryRemoveAllValues(d->cfdict);
}

uint KWQPtrDictImpl::count() const
{
    return CFDictionaryGetCount(d->cfdict);
}

void KWQPtrDictImpl::insert(void *key, const void *value)
{
    CFDictionarySetValue(d->cfdict, key, value);
}

bool KWQPtrDictImpl::remove(void *key, bool deleteItem)
{
    void *value = find(key);

    if (!value) {
        return false;
    }

    CFDictionaryRemoveValue(d->cfdict, key);
    
    if (deleteItem) {
        d->del(value);
    }
    
    for (KWQPtrDictIteratorPrivate *it = d->iterators; it; it = it->next) {
        it->remove(key);
    }

    return true;
}

void *KWQPtrDictImpl::find(void *key) const
{
    return (void *)CFDictionaryGetValue(d->cfdict, key);
}

void KWQPtrDictImpl::swap(KWQPtrDictImpl &di)
{
    KWQPtrDictPrivate *tmp;

    tmp = di.d;
    di.d = d;
    d = tmp;
}

KWQPtrDictImpl &KWQPtrDictImpl::assign(const KWQPtrDictImpl &di, bool deleteItems)
{
    KWQPtrDictImpl tmp(di);

    if (deleteItems) {
        clear(true);
    }

    swap(tmp);

    return *this;
}


void *KWQPtrDictImpl::take(void *key)
{
    void *value = find(key);
    remove(key, false);
    return value;
}





KWQPtrDictIteratorPrivate::KWQPtrDictIteratorPrivate(KWQPtrDictPrivate *d) :
    count(CFDictionaryGetCount(d->cfdict)),
    pos(0),
    keys(new void * [count]),
    values(new void * [count]),
    dict(d),
    next(d->iterators),
    prev(0)
{
    d->iterators = this;
    if (next) {
        next->prev = this;
    }
    
    CFDictionaryGetKeysAndValues(d->cfdict, (const void **)keys, (const void **)values);
}

KWQPtrDictIteratorPrivate::~KWQPtrDictIteratorPrivate()
{
    if (prev) {
        prev->next = next;
    } else if (dict) {
        dict->iterators = next;
    }
    if (next) {
        next->prev = prev;
    }
    
    delete [] keys;
    delete [] values;
}

KWQPtrDictIteratorImpl::KWQPtrDictIteratorImpl(const KWQPtrDictImpl &di) : 
    d(new KWQPtrDictIteratorPrivate(di.d))
{
}

KWQPtrDictIteratorImpl::~KWQPtrDictIteratorImpl()
{
    delete d;
}

uint KWQPtrDictIteratorImpl::count() const
{
    return d->count;
}

void *KWQPtrDictIteratorImpl::current() const
{
    if (d->pos >= d->count) {
        return NULL;
    }
    return d->values[d->pos];
}

void *KWQPtrDictIteratorImpl::currentKey() const
{
    if (d->pos >= d->count) {
        return NULL;
    }
    return d->keys[d->pos];
}

void *KWQPtrDictIteratorImpl::toFirst()
{
    d->pos = 0;
    return current();
}

void *KWQPtrDictIteratorImpl::operator++()
{
    ++d->pos;
    return current();
}

void KWQPtrDictIteratorPrivate::remove(void *key)
{
    for (uint i = 0; i < count; ) {
        if (keys[i] != key) {
            ++i;
        } else {
            --count;
            if (pos > i) {
                --pos;
            }
            memmove(&keys[i], &keys[i+1], sizeof(keys[i]) * (count - i));
            memmove(&values[i], &values[i+1], sizeof(values[i]) * (count - i));
        }
    }
}

void KWQPtrDictIteratorPrivate::dictDestroyed()
{
    count = 0;
    dict = 0;
}
