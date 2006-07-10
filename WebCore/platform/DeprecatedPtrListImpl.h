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

#ifndef DeprecatedPtrListImpl_h
#define DeprecatedPtrListImpl_h

namespace WebCore {

class DeprecatedListNode;
class DeprecatedPtrListImplIterator;

class DeprecatedPtrListImpl
{
public:
    
    DeprecatedPtrListImpl(void (*deleteFunc)(void *));
    DeprecatedPtrListImpl(const DeprecatedPtrListImpl &impl);
    ~DeprecatedPtrListImpl();
     
    bool isEmpty() const { return nodeCount == 0; }
    unsigned count() const { return nodeCount; }
    void clear(bool deleteItems);

    void *at(unsigned n);

    bool insert(unsigned n, const void *item);
    bool remove(bool deleteItem);
    bool remove(unsigned n, bool deleteItem);
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
    void *take(unsigned n);
    void *take();

    void append(const void *item);
    void prepend(const void *item);

    unsigned containsRef(const void *item) const;
    int findRef(const void *item);

    DeprecatedPtrListImpl &assign(const DeprecatedPtrListImpl &impl, bool deleteItems);

 private:
    DeprecatedPtrListImpl &operator =(const DeprecatedPtrListImpl &impl);

    void swap(DeprecatedPtrListImpl &impl);

    void addIterator(DeprecatedPtrListImplIterator *iter) const;
    void removeIterator(DeprecatedPtrListImplIterator *iter) const;

    DeprecatedListNode *head;
    DeprecatedListNode *tail;
    DeprecatedListNode *cur;
    unsigned nodeCount;
    void (*deleteItem)(void *);
    mutable DeprecatedPtrListImplIterator *iterators;

    friend class DeprecatedPtrListImplIterator;
};


class DeprecatedPtrListImplIterator {
public:
    DeprecatedPtrListImplIterator();
    DeprecatedPtrListImplIterator(const DeprecatedPtrListImpl &impl);
    ~DeprecatedPtrListImplIterator();

    DeprecatedPtrListImplIterator(const DeprecatedPtrListImplIterator &impl);
    DeprecatedPtrListImplIterator &operator=(const DeprecatedPtrListImplIterator &impl);

    unsigned count() const;
    void *toFirst();
    void *toLast();
    void *current() const;

    void *operator--();
    void *operator++();

private:
    const DeprecatedPtrListImpl *list;
    DeprecatedListNode *node;
    DeprecatedPtrListImplIterator *next;
    DeprecatedPtrListImplIterator *prev;

    friend class DeprecatedPtrListImpl;
};

}

#endif
