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

#include <KWQArrayImpl.h>

#ifndef USING_BORROWED_QARRAY

#include <cstring>
#include <new>

#define	MIN(a,b) (((a)<(b))?(a):(b))

using namespace std;

class KWQArrayImpl::KWQArrayPrivate
{
public:	
    KWQArrayPrivate(size_t pNumItems, size_t pItemSize);
    ~KWQArrayPrivate();
    size_t numItems;
    size_t itemSize;
    void *data;
    int refCount;
};

KWQArrayImpl::KWQArrayPrivate::KWQArrayPrivate(size_t pItemSize, size_t pNumItems) : 
    numItems(pNumItems), 
    itemSize(pItemSize), 
    data(pNumItems > 0 ? (void *)new char[itemSize * numItems] : NULL), 
    refCount(0)
{
}

KWQArrayImpl::KWQArrayPrivate::~KWQArrayPrivate()
{
    if (data != NULL) {
        delete[] (char *)data;
    }
}


KWQArrayImpl::KWQArrayImpl(size_t itemSize) : 
    d(new KWQArrayPrivate(itemSize, 0))
{
}

KWQArrayImpl::KWQArrayImpl(size_t itemSize, size_t numItems) : 
    d(new KWQArrayPrivate(itemSize, numItems))
{
}

KWQArrayImpl::KWQArrayImpl(const KWQArrayImpl &a) : 
    d(a.d)
{
}

KWQArrayImpl::~KWQArrayImpl()
{
}

KWQArrayImpl &KWQArrayImpl::operator=(const KWQArrayImpl &a)
{
    d = a.d;
    return *this;
}

void *KWQArrayImpl::at(size_t pos) const
{
    return (void *)&((char *)d->data)[pos*d->itemSize];
}

void *KWQArrayImpl::data() const
{
    return d->data;
}

uint KWQArrayImpl::size() const
{
    return d->numItems;
}

bool KWQArrayImpl::resize(size_t newSize)
{
    if (newSize != d->numItems) {
        void *newData;
	if (newSize != 0) {
	    newData= new(nothrow) char[newSize*d->itemSize];
	    if (newData == NULL) {
	        return false;
	    }
	} else {
	    newData = NULL;
	}

	memcpy(newData, d->data, MIN(newSize, d->numItems)*d->itemSize);
	if (d->data != NULL) {
	    delete[] (char *)d->data;
	}
	d->data = newData;
	d->numItems = newSize;
    }

    return true;
}

void KWQArrayImpl::duplicate(const void *data, size_t newSize)
{
    if (data == NULL) {
	newSize = 0;
    }

    if (d->refCount > 1) {
        d =  KWQRefPtr<KWQArrayImpl::KWQArrayPrivate>(new KWQArrayImpl::KWQArrayPrivate(d->itemSize, newSize));
    }

    if (d->numItems != newSize) {
	resize(newSize);
    }

    memcpy(d->data, data, newSize*d->itemSize);
}

bool KWQArrayImpl::fill(const void *item, int numItems)
{
    if (numItems == -1) {
        numItems = d->numItems;
    }

    if ((unsigned)numItems != d->numItems) {
        if (!resize(numItems)) {
	    return false;
	}
    }

    for(int i = 0; i < numItems; i++) {
        memcpy(&((char *)d->data)[i*d->itemSize], item, d->itemSize);
    }

    return true;
}

bool KWQArrayImpl::operator==(const KWQArrayImpl &a) const
{
    return d->numItems == a.d->numItems && d->itemSize == d->itemSize && (d->data == a.d->data || memcmp(d->data, a.d->data, d->itemSize*d->numItems) == 0);
}

#endif
