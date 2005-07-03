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

#ifndef KWQVALUELIST_H_
#define KWQVALUELIST_H_

#include "KWQRefPtr.h"
#include "KWQDef.h"

class KWQValueListNodeImpl;

class KWQValueListIteratorImpl
{
public: 
    KWQValueListIteratorImpl();
    
    bool operator==(const KWQValueListIteratorImpl &other);
    bool operator!=(const KWQValueListIteratorImpl &other);

    KWQValueListNodeImpl *node();
    const KWQValueListNodeImpl *node() const;

    KWQValueListIteratorImpl& operator++();
    KWQValueListIteratorImpl operator++(int);
    KWQValueListIteratorImpl& operator--();

private:
    KWQValueListIteratorImpl(const KWQValueListNodeImpl *n);

    KWQValueListNodeImpl *nodeImpl;

    friend class KWQValueListImpl;
};


class KWQValueListImpl 
{
public:
    KWQValueListImpl(void (*deleteFunc)(KWQValueListNodeImpl *), KWQValueListNodeImpl *(*copyNode)(KWQValueListNodeImpl *));
    ~KWQValueListImpl();
    
    KWQValueListImpl(const KWQValueListImpl&);
    KWQValueListImpl& operator=(const KWQValueListImpl&);
        
    void clear();
    uint count() const;
    bool isEmpty() const;

    KWQValueListIteratorImpl appendNode(KWQValueListNodeImpl *node);
    KWQValueListIteratorImpl prependNode(KWQValueListNodeImpl *node);
    void removeEqualNodes(KWQValueListNodeImpl *node, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *));
    uint containsEqualNodes(KWQValueListNodeImpl *node, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *)) const;
    
    KWQValueListIteratorImpl findEqualNode(KWQValueListNodeImpl *node, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *)) const;

    KWQValueListIteratorImpl insert(const KWQValueListIteratorImpl &iterator, KWQValueListNodeImpl* node);
    KWQValueListIteratorImpl removeIterator(KWQValueListIteratorImpl &iterator);
    KWQValueListIteratorImpl fromLast();

    KWQValueListNodeImpl *firstNode();
    KWQValueListNodeImpl *lastNode();

    KWQValueListNodeImpl *firstNode() const;
    KWQValueListNodeImpl *lastNode() const;

    KWQValueListIteratorImpl begin();
    KWQValueListIteratorImpl end();

    KWQValueListIteratorImpl begin() const;
    KWQValueListIteratorImpl end() const;
    KWQValueListIteratorImpl fromLast() const;
    
    KWQValueListNodeImpl *nodeAt(uint index);
    KWQValueListNodeImpl *nodeAt(uint index) const;
    
    bool isEqual(const KWQValueListImpl &other, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *)) const;
    
private:
    void copyOnWrite();

    class KWQValueListPrivate;

    KWQRefPtr<KWQValueListPrivate> d;
    
    friend class KWQValueListNodeImpl;
};

class KWQValueListNodeImpl
{
protected:
    KWQValueListNodeImpl();

private:
    KWQValueListNodeImpl *prev;
    KWQValueListNodeImpl *next;

    friend class KWQValueListImpl;
    friend class KWQValueListIteratorImpl;
    friend class KWQValueListImpl::KWQValueListPrivate;
};

#endif
