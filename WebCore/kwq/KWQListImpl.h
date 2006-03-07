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

#ifndef KWQ_LIST_IMPL_H
#define KWQ_LIST_IMPL_H

#include "KWQDef.h"

class KWQListNode;
class KWQListIteratorImpl;

class KWQListImpl
{
public:
    
    KWQListImpl(void (*deleteFunc)(void *));
    KWQListImpl(const KWQListImpl &impl);
    ~KWQListImpl();
     
    bool isEmpty() const { return nodeCount == 0; }
    uint count() const { return nodeCount; }
    void clear(bool deleteItems);

    void *at(uint n);

    bool insert(uint n, const void *item);
    bool remove(bool deleteItem);
    bool remove(uint n, bool deleteItem);
    bool removeFirst(bool deleteItem);
    bool removeLast(bool deleteItem);
    bool removeRef(const void *item, bool deleteItem);

    void *getFirst() const;
    void *getLast() const;
    void *getNext() const;
    void *getPrev() const;
    void *current() const;
    void *first();
    void *last();
    void *next();
    void *prev();
    void *take(uint n);
    void *take();

    void append(const void *item);
    void prepend(const void *item);

    uint containsRef(const void *item) const;
    int findRef(const void *item);

    KWQListImpl &assign(const KWQListImpl &impl, bool deleteItems);

 private:
    KWQListImpl &operator =(const KWQListImpl &impl);

    void swap(KWQListImpl &impl);

    void addIterator(KWQListIteratorImpl *iter) const;
    void removeIterator(KWQListIteratorImpl *iter) const;

    KWQListNode *head;
    KWQListNode *tail;
    KWQListNode *cur;
    uint nodeCount;
    void (*deleteItem)(void *);
    mutable KWQListIteratorImpl *iterators;

    friend class KWQListIteratorImpl;
};


class KWQListIteratorImpl {
public:
    KWQListIteratorImpl();
    KWQListIteratorImpl(const KWQListImpl &impl);
    ~KWQListIteratorImpl();

    KWQListIteratorImpl(const KWQListIteratorImpl &impl);
    KWQListIteratorImpl &operator=(const KWQListIteratorImpl &impl);

    uint count() const;
    void *toFirst();
    void *toLast();
    void *current() const;

    void *operator--();
    void *operator++();

private:
    const KWQListImpl *list;
    KWQListNode *node;
    KWQListIteratorImpl *next;
    KWQListIteratorImpl *prev;

    friend class KWQListImpl;
};

#endif
