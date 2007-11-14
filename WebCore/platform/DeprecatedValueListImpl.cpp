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
#include "DeprecatedValueListImpl.h"

#include <wtf/RefCounted.h>
#include <stdlib.h>

namespace WebCore {

class DeprecatedValueListImpl::Private : public RefCounted<DeprecatedValueListImpl::Private>
{
public:
    Private(void (*deleteFunc)(DeprecatedValueListImplNode *), DeprecatedValueListImplNode *(*copyFunc)(DeprecatedValueListImplNode *));
    Private(const Private &other);

    ~Private();

    void copyList(DeprecatedValueListImplNode *l, DeprecatedValueListImplNode *&head, DeprecatedValueListImplNode *&tail) const;
    void deleteList(DeprecatedValueListImplNode *l);

    DeprecatedValueListImplNode *head;
    DeprecatedValueListImplNode *tail;

    void (*deleteNode)(DeprecatedValueListImplNode *);
    DeprecatedValueListImplNode *(*copyNode)(DeprecatedValueListImplNode *);
    unsigned count;
};

inline DeprecatedValueListImpl::Private::Private(void (*deleteFunc)(DeprecatedValueListImplNode*),
        DeprecatedValueListImplNode* (*copyFunc)(DeprecatedValueListImplNode*)) : 
    head(NULL),
    tail(NULL),
    deleteNode(deleteFunc),
    copyNode(copyFunc),
    count(0)
{
}

inline DeprecatedValueListImpl::Private::Private(const Private &other)
    : RefCounted<DeprecatedValueListImpl::Private>()
    , deleteNode(other.deleteNode)
    , copyNode(other.copyNode)
    , count(other.count)
{
    other.copyList(other.head, head, tail);
}

inline DeprecatedValueListImpl::Private::~Private()
{
    deleteList(head);
}

void DeprecatedValueListImpl::Private::copyList(DeprecatedValueListImplNode *l, DeprecatedValueListImplNode *&head, DeprecatedValueListImplNode *&tail) const
{
    DeprecatedValueListImplNode *prev = NULL;
    DeprecatedValueListImplNode *node = l;

    head = NULL;

    while (node != NULL) {
        DeprecatedValueListImplNode *copy = copyNode(node);
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

void DeprecatedValueListImpl::Private::deleteList(DeprecatedValueListImplNode *l)
{
    DeprecatedValueListImplNode *p = l;
    
    while (p != NULL) {
        DeprecatedValueListImplNode *next = p->next;
        deleteNode(p);
        p = next;
    }
}

DeprecatedValueListImpl::DeprecatedValueListImpl(void (*deleteFunc)(DeprecatedValueListImplNode *), DeprecatedValueListImplNode *(*copyFunc)(DeprecatedValueListImplNode *)) :
    d(new Private(deleteFunc, copyFunc))
{
}

DeprecatedValueListImpl::DeprecatedValueListImpl(const DeprecatedValueListImpl &other) :
    d(other.d)
{
}

DeprecatedValueListImpl::~DeprecatedValueListImpl()
{
}

void DeprecatedValueListImpl::clear()
{
    if (d->head) {
        copyOnWrite();
        d->deleteList(d->head);
        d->head = NULL;
        d->tail = NULL;
        d->count = 0;
    }
}

unsigned DeprecatedValueListImpl::count() const
{
    return d->count;
}

bool DeprecatedValueListImpl::isEmpty() const
{
    return d->count == 0;
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::appendNode(DeprecatedValueListImplNode *node)
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

DeprecatedValueListImplIterator DeprecatedValueListImpl::prependNode(DeprecatedValueListImplNode *node)
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

void DeprecatedValueListImpl::removeEqualNodes(DeprecatedValueListImplNode *node, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *))
{
    copyOnWrite();

    DeprecatedValueListImplNode *next;
    for (DeprecatedValueListImplNode *p = d->head; p != NULL; p = next) {
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

unsigned DeprecatedValueListImpl::containsEqualNodes(DeprecatedValueListImplNode *node, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *)) const
{
    unsigned contains = 0;

    for (DeprecatedValueListImplNode *p = d->head; p != NULL; p = p->next) {
        if (equalFunc(node, p)) {
            ++contains;
        }
    }
    
    return contains;
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::findEqualNode(DeprecatedValueListImplNode *node, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *)) const
{
    DeprecatedValueListImplIterator it = begin();
    DeprecatedValueListImplIterator endIt = end();
    while (it != endIt) {
        if (equalFunc(node, it.node())) {
            break;
        }
        it++;
    }
    return it;
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::insert(const DeprecatedValueListImplIterator &iterator, DeprecatedValueListImplNode *node)
{
    copyOnWrite();
    
    DeprecatedValueListImplNode *next = iterator.nodeImpl;
    
    if (next == NULL)
        return appendNode(node);
    
    if (next == d->head)
        return prependNode(node);
    
    DeprecatedValueListImplNode *prev = next->prev;
    
    node->next = next;
    node->prev = prev;
    next->prev = node;
    prev->next = node;
    
    d->count++;
    
    return node;
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::removeIterator(DeprecatedValueListImplIterator &iterator)
{
    copyOnWrite();

    if (iterator.nodeImpl == NULL) {
        return iterator;
    }

    DeprecatedValueListImplNode *next = iterator.nodeImpl->next;

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

    return DeprecatedValueListImplIterator(next);
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::fromLast()
{
    copyOnWrite();
    return DeprecatedValueListImplIterator(lastNode());
}

DeprecatedValueListImplNode *DeprecatedValueListImpl::firstNode()
{
    copyOnWrite();
    return ((const DeprecatedValueListImpl *)this)->firstNode();
}

DeprecatedValueListImplNode *DeprecatedValueListImpl::lastNode()
{
    copyOnWrite();
    return ((const DeprecatedValueListImpl *)this)->lastNode();
}

DeprecatedValueListImplNode *DeprecatedValueListImpl::firstNode() const
{
    return d->head;
}

DeprecatedValueListImplNode *DeprecatedValueListImpl::lastNode() const
{
    return d->tail;
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::begin()
{
    copyOnWrite();
    return ((const DeprecatedValueListImpl *)this)->begin();
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::end()
{
    copyOnWrite();
    return ((const DeprecatedValueListImpl *)this)->end();
}


DeprecatedValueListImplIterator DeprecatedValueListImpl::begin() const
{
    return DeprecatedValueListImplIterator(firstNode());
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::end() const
{
    return DeprecatedValueListImplIterator(NULL);
}

DeprecatedValueListImplIterator DeprecatedValueListImpl::fromLast() const
{
    return DeprecatedValueListImplIterator(lastNode());
}

DeprecatedValueListImplNode *DeprecatedValueListImpl::nodeAt(unsigned index)
{
    copyOnWrite();

    if (d->count <= index) {
        return NULL;
    }

    DeprecatedValueListImplNode *p = d->head;

    for (unsigned i = 0; i < index; i++) {
        p = p->next;
    }

    return p;
}

DeprecatedValueListImplNode *DeprecatedValueListImpl::nodeAt(unsigned index) const
{
    if (d->count <= index) {
        return NULL;
    }

    DeprecatedValueListImplNode *p = d->head;

    for (unsigned i = 0; i < index; i++) {
        p = p->next;
    }

    return p;
}

DeprecatedValueListImpl& DeprecatedValueListImpl::operator=(const DeprecatedValueListImpl &other)
{
    DeprecatedValueListImpl tmp(other);
    RefPtr<Private> tmpD = tmp.d;

    tmp.d = d;
    d = tmpD;

    return *this;
}

void DeprecatedValueListImpl::copyOnWrite()
{
    if (!d->hasOneRef())
        d = new Private(*d);
}

bool DeprecatedValueListImpl::isEqual(const DeprecatedValueListImpl &other, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *)) const
{
    DeprecatedValueListImplNode *p, *q;
    for (p = d->head, q = other.d->head; p && q; p = p->next, q = q->next) {
        if (!equalFunc(p, q)) {
            return false;
        }
    }
    return !p && !q;
}

}
