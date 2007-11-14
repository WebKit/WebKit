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

#ifndef ArrayImpl_h
#define ArrayImpl_h

#include <wtf/RefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ArrayImpl
{
 public:
    ArrayImpl(size_t itemSize, size_t numItems = 0);
    ~ArrayImpl();
    
    ArrayImpl(const ArrayImpl &);
    ArrayImpl &operator=(const ArrayImpl &);
    
    void *at(size_t pos) const { return &d->data[pos * d->itemSize]; }

    void *data() const;
    unsigned size() const;
    bool resize(size_t size);
    void duplicate(const void *data, size_t size);
    bool fill(const void *item, int size = -1);
    void detach();
    
    bool operator==(const ArrayImpl &) const;

 private:
    class ArrayPrivate : public RefCounted<ArrayPrivate>
    {
    public:
        ArrayPrivate(size_t pNumItems, size_t pItemSize);
        ~ArrayPrivate();

        size_t numItems;
        size_t itemSize;
        char *data;
    };

    RefPtr<ArrayPrivate> d;
};

inline unsigned ArrayImpl::size() const
{
    return d->numItems;
}

}

#endif
