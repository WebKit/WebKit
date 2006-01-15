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
#include "KWQValueListImpl.h"

#include "Shared.h"
#include <stdlib.h>

class KWQValueListImpl::KWQValueListPrivate : public Shared<KWQValueListImpl::KWQValueListPrivate>
{
public:
    KWQValueListPrivate(void (*deleteFunc)(KWQValueListNodeImpl *), KWQValueListNodeImpl *(*copyFunc)(KWQValueListNodeImpl *));
    KWQValueListPrivate(const KWQValueListPrivate &other);

    ~KWQValueListPrivate();

    void copyList(KWQValueListNodeImpl *l, KWQValueListNodeImpl *&head, KWQValueListNodeImpl *&tail) const;
    void deleteList(KWQValueListNodeImpl *l);

    KWQValueListNodeImpl *head;
    KWQValueListNodeImpl *tail;

    void (*deleteNode)(KWQValueListNodeImpl *);
    KWQValueListNodeImpl *(*copyNode)(KWQValueListNodeImpl *);
    uint count;
};

inline KWQValueListImpl::KWQValueListPrivate::KWQValueListPrivate(void (*deleteFunc)(KWQValueListNodeImpl *), 
							   KWQValueListNodeImpl *(*copyFunc)(KWQValueListNodeImpl *)) : 
    head(NULL),
    tail(NULL),
    deleteNode(deleteFunc),
    copyNode(copyFunc),
    count(0)
{
}

inline KWQValueListImpl::KWQValueListPrivate::KWQValueListPrivate(const KWQValueListPrivate &other) :
    Shared<KWQValueListImpl::KWQValueListPrivate>(),
    deleteNode(other.deleteNode),
    copyNode(other.copyNode),
    count(other.count)
{
    other.copyList(other.head, head, tail);
}

inline KWQValueListImpl::KWQValueListPrivate::~KWQValueListPrivate()
{
    deleteList(head);
}

void KWQValueListImpl::KWQValueListPrivate::copyList(KWQValueListNodeImpl *l, KWQValueListNodeImpl *&head, KWQValueListNodeImpl *&tail) const
{
    KWQValueListNodeImpl *prev = NULL;
    KWQValueListNodeImpl *node = l;

    head = NULL;

    while (node != NULL) {
	KWQValueListNodeImpl *copy = copyNode(node);
	if (prev == NULL) {
	    head = copy;
	} else {
	    prev->next = copy;
	}

	copy->prev = prev;
	copy->next = NULL;

	prev = copy;
	node = node->next;
    }

    tail = prev;
}

void KWQValueListImpl::KWQValueListPrivate::deleteList(KWQValueListNodeImpl *l)
{
    KWQValueListNodeImpl *p = l;
    
    while (p != NULL) {
	KWQValueListNodeImpl *next = p->next;
	deleteNode(p);
	p = next;
    }
}

KWQValueListImpl::KWQValueListImpl(void (*deleteFunc)(KWQValueListNodeImpl *), KWQValueListNodeImpl *(*copyFunc)(KWQValueListNodeImpl *)) :
    d(new KWQValueListPrivate(deleteFunc, copyFunc))
{
}

KWQValueListImpl::KWQValueListImpl(const KWQValueListImpl &other) :
    d(other.d)
{
}

KWQValueListImpl::~KWQValueListImpl()
{
}

void KWQValueListImpl::clear()
{
    if (d->head) {
        copyOnWrite();
        d->deleteList(d->head);
        d->head = NULL;
        d->tail = NULL;
        d->count = 0;
    }
}

uint KWQValueListImpl::count() const
{
    return d->count;
}

bool KWQValueListImpl::isEmpty() const
{
    return d->count == 0;
}

KWQValueListIteratorImpl KWQValueListImpl::appendNode(KWQValueListNodeImpl *node)
{
    copyOnWrite();

    node->next = NULL;
    node->prev = d->tail;
    d->tail = node;
    
    if (d->head == NULL) {
        d->head = node;
    } else {
        node->prev->next = node;
    }

    d->count++;
    
    return node;
}

KWQValueListIteratorImpl KWQValueListImpl::prependNode(KWQValueListNodeImpl *node)
{
    copyOnWrite();

    node->next = d->head;
    node->prev = NULL;
    d->head = node;

    if (d->tail == NULL) {
        d->tail = node;
    } else {
	node->next->prev = node;
    }

    d->count++;
    
    return node;
}

void KWQValueListImpl::removeEqualNodes(KWQValueListNodeImpl *node, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *))
{
    copyOnWrite();

    KWQValueListNodeImpl *next;
    for (KWQValueListNodeImpl *p = d->head; p != NULL; p = next) {
	next = p->next;
	if (equalFunc(node, p)) {
	    if (p->next != NULL) {
		p->next->prev = p->prev;
	    } else {
                d->tail = p->prev;
            }

	    if (p->prev != NULL) {
		p->prev->next = p->next;
	    } else {
		d->head = p->next;
	    }

	    d->deleteNode(p);

	    d->count--;
	}
    }
}

uint KWQValueListImpl::containsEqualNodes(KWQValueListNodeImpl *node, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *)) const
{
    unsigned contains = 0;

    for (KWQValueListNodeImpl *p = d->head; p != NULL; p = p->next) {
	if (equalFunc(node, p)) {
	    ++contains;
	}
    }
    
    return contains;
}

KWQValueListIteratorImpl KWQValueListImpl::findEqualNode(KWQValueListNodeImpl *node, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *)) const
{
    KWQValueListIteratorImpl it = begin();
    KWQValueListIteratorImpl endIt = end();
    while (it != endIt) {
        if (equalFunc(node, it.node())) {
            break;
        }
        it++;
    }
    return it;
}

KWQValueListIteratorImpl KWQValueListImpl::insert(const KWQValueListIteratorImpl &iterator, KWQValueListNodeImpl *node)
{
    copyOnWrite();
    
    KWQValueListNodeImpl *next = iterator.nodeImpl;
    
    if (next == NULL)
        return appendNode(node);
    
    if (next == d->head)
        return prependNode(node);
    
    KWQValueListNodeImpl *prev = next->prev;
    
    node->next = next;
    node->prev = prev;
    next->prev = node;
    prev->next = node;
    
    d->count++;
    
    return node;
}

KWQValueListIteratorImpl KWQValueListImpl::removeIterator(KWQValueListIteratorImpl &iterator)
{
    copyOnWrite();

    if (iterator.nodeImpl == NULL) {
	return iterator;
    }

    KWQValueListNodeImpl *next = iterator.nodeImpl->next;

    // detach node
    if (iterator.nodeImpl->next != NULL) {
	iterator.nodeImpl->next->prev = iterator.nodeImpl->prev;
    } else {
        d->tail = iterator.nodeImpl->prev;
    }
    if (iterator.nodeImpl->prev != NULL) {
	iterator.nodeImpl->prev->next = iterator.nodeImpl->next;
    } else {
	d->head = iterator.nodeImpl->next;
    }
    
    d->deleteNode(iterator.nodeImpl);
    d->count--;

    return KWQValueListIteratorImpl(next);
}

KWQValueListIteratorImpl KWQValueListImpl::fromLast()
{
    copyOnWrite();
    return KWQValueListIteratorImpl(lastNode());
}

KWQValueListNodeImpl *KWQValueListImpl::firstNode()
{
    copyOnWrite();
    return ((const KWQValueListImpl *)this)->firstNode();
}

KWQValueListNodeImpl *KWQValueListImpl::lastNode()
{
    copyOnWrite();
    return ((const KWQValueListImpl *)this)->lastNode();
}

KWQValueListNodeImpl *KWQValueListImpl::firstNode() const
{
    return d->head;
}

KWQValueListNodeImpl *KWQValueListImpl::lastNode() const
{
    return d->tail;
}

KWQValueListIteratorImpl KWQValueListImpl::begin()
{
    copyOnWrite();
    return ((const KWQValueListImpl *)this)->begin();
}

KWQValueListIteratorImpl KWQValueListImpl::end()
{
    copyOnWrite();
    return ((const KWQValueListImpl *)this)->end();
}


KWQValueListIteratorImpl KWQValueListImpl::begin() const
{
    return KWQValueListIteratorImpl(firstNode());
}

KWQValueListIteratorImpl KWQValueListImpl::end() const
{
    return KWQValueListIteratorImpl(NULL);
}

KWQValueListIteratorImpl KWQValueListImpl::fromLast() const
{
    return KWQValueListIteratorImpl(lastNode());
}

KWQValueListNodeImpl *KWQValueListImpl::nodeAt(uint index)
{
    copyOnWrite();

    if (d->count <= index) {
	return NULL;
    }

    KWQValueListNodeImpl *p = d->head;

    for (uint i = 0; i < index; i++) {
	p = p->next;
    }

    return p;
}

KWQValueListNodeImpl *KWQValueListImpl::nodeAt(uint index) const
{
    if (d->count <= index) {
	return NULL;
    }

    KWQValueListNodeImpl *p = d->head;

    for (uint i = 0; i < index; i++) {
	p = p->next;
    }

    return p;
}

KWQValueListImpl& KWQValueListImpl::operator=(const KWQValueListImpl &other)
{
    KWQValueListImpl tmp(other);
    RefPtr<KWQValueListPrivate> tmpD = tmp.d;

    tmp.d = d;
    d = tmpD;

    return *this;
}

void KWQValueListImpl::copyOnWrite()
{
    if (!d->hasOneRef())
	d = new KWQValueListPrivate(*d);
}

bool KWQValueListImpl::isEqual(const KWQValueListImpl &other, bool (*equalFunc)(const KWQValueListNodeImpl *, const KWQValueListNodeImpl *)) const
{
    KWQValueListNodeImpl *p, *q;
    for (p = d->head, q = other.d->head; p && q; p = p->next, q = q->next) {
	if (!equalFunc(p, q)) {
	    return false;
	}
    }
    return !p && !q;
}
