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

#ifndef KWQ_VECTOR_IMPL_H
#define KWQ_VECTOR_IMPL_H

#include <config.h>
#include <KWQDef.h>

#ifndef USING_BORROWED_QVECTOR

class KWQVectorImpl
{
 public:
    KWQVectorImpl(void (*deleteFunc)(void *));
    KWQVectorImpl(uint size, void (*deleteFunc)(void *));
    KWQVectorImpl(const KWQVectorImpl &vi);
    ~KWQVectorImpl();

    void clear(bool delItems);
    bool isEmpty() const;
    uint count() const;
    uint size() const;
    bool remove(uint n, bool delItems); 
    bool resize(uint size, bool delItems);
    bool insert(uint n, const void *item, bool delItems);
    void *at(int n) const;
    void **data();
    KWQVectorImpl &assign (KWQVectorImpl &vi, bool delItems);

 private:
    void KWQVectorImpl::swap(KWQVectorImpl &di);

    class KWQVectorPrivate;
    KWQVectorPrivate *d;
};


#endif

#endif
