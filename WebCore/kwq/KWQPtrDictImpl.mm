/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <KWQPtrDictImpl.h>

#ifndef USING_BORROWED_QPTRDICT

#include <new>

typedef void (* DeleteFunction) (void *);

class KWQPtrDictImpl::KWQPtrDictPrivate
{
public:
    KWQPtrDictPrivate(int size, DeleteFunction, const CFDictionaryKeyCallBacks *cfdkcb);
    KWQPtrDictPrivate(KWQPtrDictPrivate &dp);
    ~KWQPtrDictPrivate();
    
    CFMutableDictionaryRef cfdict;
    DeleteFunction del;
};

KWQPtrDictImpl::KWQPtrDictPrivate::KWQPtrDictPrivate(int size, DeleteFunction deleteFunc, const CFDictionaryKeyCallBacks *cfdkcb) :
    cfdict(CFDictionaryCreateMutable(NULL, 0, cfdkcb, NULL)),
    del(deleteFunc)
{
    if (cfdict == NULL) {
	throw std::bad_alloc();
    }
}

KWQPtrDictImpl::KWQPtrDictPrivate::KWQPtrDictPrivate(KWQPtrDictPrivate &dp) :     
    cfdict(CFDictionaryCreateMutableCopy(NULL, 0, dp.cfdict)),
    del(dp.del)
{
    if (cfdict == NULL) {
	throw std::bad_alloc();
    }
}

KWQPtrDictImpl::KWQPtrDictPrivate::~KWQPtrDictPrivate()
{
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
    void *value = (void *)CFDictionaryGetValue(d->cfdict, key);

    if (value == nil) {
	return false;
    }

    CFDictionaryRemoveValue(d->cfdict, key);
	
    if (deleteItem) {
	d->del(value);
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





class KWQPtrDictIteratorImpl::KWQPtrDictIteratorPrivate
{
public:
    KWQPtrDictIteratorPrivate(CFMutableDictionaryRef cfdict);
    ~KWQPtrDictIteratorPrivate();
    uint count;
    uint pos;
    void **keys;
    void **values;
};


KWQPtrDictIteratorImpl::KWQPtrDictIteratorPrivate::KWQPtrDictIteratorPrivate(CFMutableDictionaryRef cfdict) :
    count(CFDictionaryGetCount(cfdict)),
    pos(0),
    keys(new void*[count]),
    values(new void*[count])
{
    CFDictionaryGetKeysAndValues(cfdict, (const void **)keys, (const void **)values);
}

KWQPtrDictIteratorImpl::KWQPtrDictIteratorPrivate::~KWQPtrDictIteratorPrivate()
{
    delete keys;
    delete values;
}


KWQPtrDictIteratorImpl::KWQPtrDictIteratorImpl(const KWQPtrDictImpl &di) : 
    d(new KWQPtrDictIteratorImpl::KWQPtrDictIteratorPrivate(di.d->cfdict))
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

void * KWQPtrDictIteratorImpl::currentKey() const
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
    ++(d->pos);
    return current();
}

#endif
