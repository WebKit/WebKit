/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "KWQListImpl.h"

#include <cstddef>
#include <algorithm>
#include <kxmlcore/Assertions.h>

class KWQListNode
{
public:
    KWQListNode(void *d) : data(d), next(0), prev(0) { }

    void *data;
    KWQListNode *next;
    KWQListNode *prev;
};


static KWQListNode *copyList(KWQListNode *l, KWQListNode *&tail)
{
    KWQListNode *node = l;
    KWQListNode *copyHead = 0;
    KWQListNode *last = 0;

    while (node != 0) {
	KWQListNode *copy = new KWQListNode(node->data);
	if (last != 0) {
	    last->next = copy;
	} else {
	    copyHead = copy;
	}

	copy->prev = last;
	
	last = copy;
	node = node->next;
    }

    tail = last;
    return copyHead;
}


KWQListImpl::KWQListImpl(void (*deleteFunc)(void *)) :
    head(0),
    tail(0),
    cur(0),
    nodeCount(0),
    deleteItem(deleteFunc),
    iterators(0)
{
}

KWQListImpl::KWQListImpl(const KWQListImpl &impl) :
    cur(0),
    nodeCount(impl.nodeCount),
    deleteItem(impl.deleteItem),
    iterators(0)
{
    head = copyList(impl.head, tail);
}

KWQListImpl::~KWQListImpl()
{
    clear(false);
    
    KWQListIteratorImpl *next;
    for (KWQListIteratorImpl *it = iterators; it; it = next) {
        next = it->next;
        it->list = 0;
        ASSERT(!it->node);
        it->next = 0;
        it->prev = 0;
    }
}
     
void KWQListImpl::clear(bool deleteItems)
{
    KWQListNode *next;
    
    for (KWQListNode *node = head; node; node = next) {
        next = node->next;
        if (deleteItems)
            deleteItem(node->data);
        delete node;
    }

    head = 0;
    tail = 0;
    cur = 0;
    nodeCount = 0;

    for (KWQListIteratorImpl *it = iterators; it; it = it->next)
	it->node = 0;
}

void *KWQListImpl::at(uint n)
{
    KWQListNode *node;
    if (n >= nodeCount - 1) {
        node = tail;
    } else {
        node = head;
        for (uint i = 0; i < n && node; i++) {
            node = node->next;
        }
    }

    cur = node;
    return node ? node->data : 0;
}

bool KWQListImpl::insert(uint n, const void *item)
{
    if (n > nodeCount) {
	return false;
    }

    KWQListNode *node = new KWQListNode((void *)item);

    if (n == 0) {
	// inserting at head
	node->next = head;
	if (head) {
	    head->prev = node;
	}
	head = node;
        if (tail == 0) {
            tail = node;
        }
    } else if (n == nodeCount) {
        // inserting at tail
        node->prev = tail;
        if (tail) {
            tail->next = node;
        }
        tail = node;
    } else {
	// general insertion
	
	// iterate to one node before the insertion point, can't be null
	// since we know n > 0 and n < nodeCount
	KWQListNode *prevNode = head;

	for (uint i = 0; i < n - 1; i++) {
	    prevNode = prevNode->next;
	}
	node->prev = prevNode;
	node->next = prevNode->next;
	if (node->next) {
	    node->next->prev = node;
	}
	prevNode->next = node;
    }

    nodeCount++;
    cur = node;
    return true;
}

bool KWQListImpl::remove(bool shouldDeleteItem)
{
    KWQListNode *node = cur;
    if (node == 0) {
	return false;
    }

    if (node->prev == 0) {
	head = node->next;
    } else {
	node->prev->next = node->next;
    }

    if (node->next == 0) {
        tail = node->prev;
    } else {
	node->next->prev = node->prev;
    }

    if (node->next) {
	cur = node->next;
    } else {
	cur = node->prev;
    }

    for (KWQListIteratorImpl *it = iterators; it; it = it->next) {
	if (it->node == node) {
	    it->node = cur;
	}
    }

    if (shouldDeleteItem) {
	deleteItem(node->data);
    }
    delete node;

    nodeCount--;

    return true;
}

bool KWQListImpl::remove(uint n, bool deleteItem)
{
    if (n >= nodeCount) {
	return false;
    }

    at(n);
    return remove(deleteItem);
}

bool KWQListImpl::removeFirst(bool deleteItem)
{
    return remove(0, deleteItem);
}

bool KWQListImpl::removeLast(bool deleteItem)
{
    return remove(nodeCount - 1, deleteItem);
}

bool KWQListImpl::remove(const void *item, bool deleteItem, int (*compareFunc)(void *a, void *b, void *data), void *data)
{
    KWQListNode *node;

    node = head;

    while (node && compareFunc((void *)item, node->data, data) != 0) {
	node = node->next;
    }
    
    if (node == 0) {
	return false;
    }

    cur = node;

    return remove(deleteItem);
}

bool KWQListImpl::removeRef(const void *item, bool deleteItem)
{
    KWQListNode *node;

    node = head;

    while (node && item != node->data) {
	node = node->next;
    }
    
    if (node == 0) {
	return false;
    }

    cur = node;

    return remove(deleteItem);
}

void *KWQListImpl::getFirst() const
{
    return head ? head->data : 0;
}

void *KWQListImpl::getLast() const
{
    return tail ? tail->data : 0;
}

void *KWQListImpl::getNext() const
{
    return cur && cur->next ? cur->next->data : 0;
}

void *KWQListImpl::getPrev() const
{
    return cur && cur->prev ? cur->prev->data : 0;
}

void *KWQListImpl::current() const
{
    if (cur) {
	return cur->data;
    } else {
	return 0;
    }
}

void *KWQListImpl::first()
{
    cur = head;
    return current();
}

void *KWQListImpl::last()
{
    cur = tail;
    return current();
}

void *KWQListImpl::next()
{
    if (cur) {
	cur = cur->next;
    }
    return current();
}

void *KWQListImpl::prev()
{
    if (cur) {
	cur = cur->prev;
    }
    return current();
}

void *KWQListImpl::take(uint n)
{
    void *retval = at(n);
    remove(false);
    return retval;
}

void *KWQListImpl::take()
{
    void *retval = current();
    remove(false);
    return retval;
}

void KWQListImpl::append(const void *item)
{
    insert(nodeCount, item);
}

void KWQListImpl::prepend(const void *item)
{
    insert(0, item);
}

uint KWQListImpl::containsRef(const void *item) const
{
    uint count = 0;
    
    for (KWQListNode *node = head; node; node = node->next) {
        if (item == node->data) {
            ++count;
        }
    }
    
    return count;
}

// Only used for KDOM::NodeImpl::compareDocumentPosition(NodeImpl *other)
// remove when no longer needed.
int KWQListImpl::findRef(const void *item)
{
    KWQListNode *node = head;
    int index = 0;
    
    while (node && item != node->data) {
        node = node->next;
        index++;
    }
    
    cur = node;
    
    if (node == 0) {
        return -1;
    }
    
    return index;
}

KWQListImpl &KWQListImpl::assign(const KWQListImpl &impl, bool deleteItems)
{
    clear(deleteItems);
    KWQListImpl(impl).swap(*this);
    return *this;
}

void KWQListImpl::addIterator(KWQListIteratorImpl *iter) const
{
    iter->next = iterators;
    iter->prev = 0;
    
    if (iterators) {
        iterators->prev = iter;
    }
    iterators = iter;
}

void KWQListImpl::removeIterator(KWQListIteratorImpl *iter) const
{
    if (iter->prev == 0) {
        iterators = iter->next;
    } else {
        iter->prev->next = iter->next;
    }

    if (iter->next) {
        iter->next->prev = iter->prev;
    }
}

void KWQListImpl::swap(KWQListImpl &other)
{
    using std::swap;
    
    ASSERT(iterators == 0);
    ASSERT(other.iterators == 0);
    
    swap(head, other.head);
    swap(tail, other.tail);
    swap(cur, other.cur);
    swap(nodeCount, other.nodeCount);
    swap(deleteItem, other.deleteItem);
}


KWQListIteratorImpl::KWQListIteratorImpl() :
    list(0),
    node(0)
{
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListImpl &impl)  :
    list(&impl),
    node(impl.head)
{
    impl.addIterator(this);
}

KWQListIteratorImpl::~KWQListIteratorImpl()
{
    if (list) {
	list->removeIterator(this);
    }
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListIteratorImpl &impl) :
    list(impl.list),
    node(impl.node)
{
    if (list) {
        list->addIterator(this);
    }
}

uint KWQListIteratorImpl::count() const
{
    return list == 0 ? 0 : list->count();
}

void *KWQListIteratorImpl::toFirst()
{
    if (list) {
        node = list->head;
    }
    return current();
}

void *KWQListIteratorImpl::toLast()
{
    if (list) {
        node = list->tail;
    }
    return current();
}

void *KWQListIteratorImpl::current() const
{
    return node == 0 ? 0 : node->data;
}

void *KWQListIteratorImpl::operator--()
{
    if (node) {
	node = node->prev;
    }
    return current();
}

void *KWQListIteratorImpl::operator++()
{
    if (node) {
	node = node->next;
    }
    return current();
}

KWQListIteratorImpl &KWQListIteratorImpl::operator=(const KWQListIteratorImpl &impl)
{
    if (list) {
	list->removeIterator(this);
    }
    
    list = impl.list;
    node = impl.node;
    
    if (list) {
	list->addIterator(this);
    }

    return *this;
}
