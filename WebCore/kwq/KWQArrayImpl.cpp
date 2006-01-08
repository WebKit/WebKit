/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQArrayImpl.h"

#import <new>
#import <string.h>

#define	MIN(a,b) (((a)<(b))?(a):(b))

using std::nothrow;

KWQArrayImpl::KWQArrayPrivate::KWQArrayPrivate(size_t pItemSize, size_t pNumItems) : 
    numItems(pNumItems), 
    itemSize(pItemSize), 
    data(pNumItems > 0 ? static_cast<char *>(fastMalloc(itemSize * numItems)) : NULL)
{
}

KWQArrayImpl::KWQArrayPrivate::~KWQArrayPrivate()
{
    fastFree(data);
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

void *KWQArrayImpl::data() const
{
    return d->data;
}

bool KWQArrayImpl::resize(size_t newSize)
{
    if (newSize != d->numItems) {
        char *newData;
        
	if (newSize != 0) {
	    newData = static_cast<char *>(fastRealloc(d->data, newSize * d->itemSize));
	    if (newData == NULL) {
	        return false;
	    }
	} else {
	    newData = NULL;
            fastFree(d->data);
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

    if (!d->hasOneRef())
        d = new KWQArrayPrivate(d->itemSize, newSize);

    if (d->numItems != newSize) {
	resize(newSize);
    }

    memcpy(d->data, data, newSize * d->itemSize);
}

void KWQArrayImpl::detach()
{
    if (!d->hasOneRef())
        duplicate(d->data, d->numItems);
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

    for (int i = 0; i < numItems; i++) {
        memcpy(&d->data[i * d->itemSize], item, d->itemSize);
    }

    return true;
}

bool KWQArrayImpl::operator==(const KWQArrayImpl &a) const
{
    return d->numItems == a.d->numItems && d->itemSize == d->itemSize
        && (d->data == a.d->data || memcmp(d->data, a.d->data, d->itemSize*d->numItems) == 0);
}
