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

#include <KWQVectorImpl.h>

#ifndef USING_BORROWED_QVECTOR

#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#include <CoreFoundation/CFArray.h>
#undef Fixed
#undef Rect
#undef Boolean

#include <new>

class KWQVectorImpl::KWQVectorPrivate
{
public:
    KWQVectorPrivate(int size, void (*deleteFunc)(void *));
    KWQVectorPrivate(KWQVectorPrivate &vp);
    ~KWQVectorPrivate();
    
    CFMutableArrayRef cfarray;
    uint size;
    uint count;
    uint max;
    void (*deleteItem)(void *);
};

KWQVectorImpl::KWQVectorPrivate::KWQVectorPrivate(int sz, void (*deleteFunc)(void *)) :
    cfarray(CFArrayCreateMutable(NULL, 0, NULL)),
    size(sz),
    count(0),
    max(0),
    deleteItem(deleteFunc)
{
    if (cfarray == NULL) {
	throw bad_alloc();
    }
}

KWQVectorImpl::KWQVectorPrivate::KWQVectorPrivate(KWQVectorPrivate &vp) :
    cfarray(CFArrayCreateMutableCopy(NULL, 0, vp.cfarray)),
    size(vp.size),
    count(vp.count),
    max(vp.max),
    deleteItem(vp.deleteItem)
{
    if (cfarray == NULL) {
	throw bad_alloc();
    }
}

KWQVectorImpl::KWQVectorPrivate::~KWQVectorPrivate()
{
    CFRelease(cfarray);
}


KWQVectorImpl::KWQVectorImpl(void (*deleteFunc)(void *)) :
    d(new KWQVectorImpl::KWQVectorPrivate(0, deleteFunc))
{
}

KWQVectorImpl::KWQVectorImpl(uint size, void (*deleteFunc)(void *)) :
    d(new KWQVectorImpl::KWQVectorPrivate(size, deleteFunc))
{
}

KWQVectorImpl::KWQVectorImpl(const KWQVectorImpl &vi) : 
    d(new KWQVectorImpl::KWQVectorPrivate(*vi.d))
{
}

KWQVectorImpl::~KWQVectorImpl()
{
    delete d;
}

void KWQVectorImpl::clear(bool delItems)
{
    if (delItems) {
	while (d->max > 0) {
	    d->max--;
	    void *item = (void *)CFArrayGetValueAtIndex(d->cfarray, d->max);
	    if (item != NULL) {
		d->deleteItem(item);
	    }
	}
    }

    CFArrayRemoveAllValues(d->cfarray);
    d->count = 0;
    d->max = 0;
}

bool KWQVectorImpl::isEmpty() const
{
    return d->count == 0;
}

uint KWQVectorImpl::count() const
{
    return d->count;
}

uint KWQVectorImpl::size() const
{
    return d->size;
}

bool KWQVectorImpl::remove(uint n, bool delItems)
{
    if (n < d->max) {
	void *item = (void *)CFArrayGetValueAtIndex(d->cfarray, n);
	
	if (item != NULL) {
	    if (delItems) {
		d->deleteItem(item);
	    }
	    
	    CFArraySetValueAtIndex(d->cfarray, n, NULL);
	    
	    d->count--;
	}
	return true;
    } else {
	return false;
    }
}

bool KWQVectorImpl::resize(uint size, bool delItems)
{
    d->size = size;

    while (d->max > size) {
	d->max--;
	void *item = (void *)CFArrayGetValueAtIndex(d->cfarray, d->max);

	if (item != NULL) {
	    if (delItems) {
		d->deleteItem(item);
	    }
	    d->count--;
	}
	CFArrayRemoveValueAtIndex(d->cfarray, d->max);
    }

    return true;
}

bool KWQVectorImpl::insert(uint n, const void *item, bool delItems)
{
    if (n >= d->size) {
	return false;
    }

    if (n < d->max) {
	void *item = (void *)CFArrayGetValueAtIndex(d->cfarray, n);
	if (item != NULL) {
	    if (delItems) {
		d->deleteItem((void *)CFArrayGetValueAtIndex(d->cfarray, n));
	    }
	} else {
	    d->count++;
	}
    }  else {
	while (n > d->max) {
	    CFArraySetValueAtIndex(d->cfarray, d->max, NULL);
	    d->max++;
	}
	d->max++;
	d->count++;
    }

    CFArraySetValueAtIndex(d->cfarray, n, item);

    return true;
}

void *KWQVectorImpl::at(int n) const
{
    if ((unsigned)n >= d->max) {
	return NULL;
    }

    return (void *)CFArrayGetValueAtIndex(d->cfarray, n);
}

KWQVectorImpl &KWQVectorImpl::assign (KWQVectorImpl &vi, bool delItems)
{
    KWQVectorImpl tmp(vi);

    swap(tmp);

    return *this;
}

void KWQVectorImpl::KWQVectorImpl::swap(KWQVectorImpl &di)
{
    KWQVectorImpl::KWQVectorPrivate *tmp = d;
    d = di.d;
    di.d = tmp;
}


#endif

