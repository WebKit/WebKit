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
#include "DeprecatedPtrListImpl.h"

#include <cstddef>
#include <algorithm>
#include <wtf/Assertions.h>

namespace WebCore {

class DeprecatedListNode
{
public:
    DeprecatedListNode(void *d) : data(d), next(0), prev(0) { }

    void *data;
    DeprecatedListNode *next;
    DeprecatedListNode *prev;
};


static DeprecatedListNode *copyList(DeprecatedListNode *l, DeprecatedListNode *&tail)
{
    DeprecatedListNode *node = l;
    DeprecatedListNode *copyHead = 0;
    DeprecatedListNode *last = 0;

    while (node != 0) {
        DeprecatedListNode *copy = new DeprecatedListNode(node->data);
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


DeprecatedPtrListImpl::DeprecatedPtrListImpl(void (*deleteFunc)(void *)) :
    head(0),
    tail(0),
    cur(0),
    nodeCount(0),
    deleteItem(deleteFunc),
    iterators(0)
{
}

DeprecatedPtrListImpl::DeprecatedPtrListImpl(const DeprecatedPtrListImpl &impl) :
    cur(0),
    nodeCount(impl.nodeCount),
    deleteItem(impl.deleteItem),
    iterators(0)
{
    head = copyList(impl.head, tail);
}

DeprecatedPtrListImpl::~DeprecatedPtrListImpl()
{
    clear(false);
    
    DeprecatedPtrListImplIterator *next;
    for (DeprecatedPtrListImplIterator *it = iterators; it; it = next) {
        next = it->next;
        it->list = 0;
        ASSERT(!it->node);
        it->next = 0;
        it->prev = 0;
    }
}
     
void DeprecatedPtrListImpl::clear(bool deleteItems)
{
    DeprecatedListNode *next;
    
    for (DeprecatedListNode *node = head; node; node = next) {
        next = node->next;
        if (deleteItems)
            deleteItem(node->data);
        delete node;
    }

    head = 0;
    tail = 0;
    cur = 0;
    nodeCount = 0;

    for (DeprecatedPtrListImplIterator *it = iterators; it; it = it->next)
        it->node = 0;
}

void *DeprecatedPtrListImpl::at(unsigned n)
{
    DeprecatedListNode *node;
    if (n >= nodeCount - 1) {
        node = tail;
    } else {
        node = head;
        for (unsigned i = 0; i < n && node; i++) {
            node = node->next;
        }
    }

    cur = node;
    return node ? node->data : 0;
}

bool DeprecatedPtrListImpl::insert(unsigned n, const void *item)
{
    if (n > nodeCount) {
        return false;
    }

    DeprecatedListNode *node = new DeprecatedListNode(const_cast<void*>(item));

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
        DeprecatedListNode *prevNode = head;

        for (unsigned i = 0; i < n - 1; i++) {
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

bool DeprecatedPtrListImpl::remove(bool shouldDeleteItem)
{
    DeprecatedListNode *node = cur;
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

    for (DeprecatedPtrListImplIterator *it = iterators; it; it = it->next) {
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

bool DeprecatedPtrListImpl::remove(unsigned n, bool deleteItem)
{
    if (n >= nodeCount) {
        return false;
    }

    at(n);
    return remove(deleteItem);
}

bool DeprecatedPtrListImpl::removeFirst(bool deleteItem)
{
    return remove(0, deleteItem);
}

bool DeprecatedPtrListImpl::removeLast(bool deleteItem)
{
    return remove(nodeCount - 1, deleteItem);
}

bool DeprecatedPtrListImpl::removeRef(const void *item, bool deleteItem)
{
    DeprecatedListNode *node;

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

void *DeprecatedPtrListImpl::getFirst() const
{
    return head ? head->data : 0;
}

void *DeprecatedPtrListImpl::getLast() const
{
    return tail ? tail->data : 0;
}

void *DeprecatedPtrListImpl::getNext() const
{
    return cur && cur->next ? cur->next->data : 0;
}

void *DeprecatedPtrListImpl::getPrev() const
{
    return cur && cur->prev ? cur->prev->data : 0;
}

void *DeprecatedPtrListImpl::current() const
{
    if (cur) {
        return cur->data;
    } else {
        return 0;
    }
}

void *DeprecatedPtrListImpl::first()
{
    cur = head;
    return current();
}

void *DeprecatedPtrListImpl::last()
{
    cur = tail;
    return current();
}

void *DeprecatedPtrListImpl::next()
{
    if (cur) {
        cur = cur->next;
    }
    return current();
}

void *DeprecatedPtrListImpl::prev()
{
    if (cur) {
        cur = cur->prev;
    }
    return current();
}

void *DeprecatedPtrListImpl::take(unsigned n)
{
    void *retval = at(n);
    remove(false);
    return retval;
}

void *DeprecatedPtrListImpl::take()
{
    void *retval = current();
    remove(false);
    return retval;
}

void DeprecatedPtrListImpl::append(const void *item)
{
    insert(nodeCount, item);
}

void DeprecatedPtrListImpl::prepend(const void *item)
{
    insert(0, item);
}

unsigned DeprecatedPtrListImpl::containsRef(const void *item) const
{
    unsigned count = 0;
    
    for (DeprecatedListNode *node = head; node; node = node->next) {
        if (item == node->data) {
            ++count;
        }
    }
    
    return count;
}

int DeprecatedPtrListImpl::findRef(const void *item)
{
    DeprecatedListNode *node = head;
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

DeprecatedPtrListImpl &DeprecatedPtrListImpl::assign(const DeprecatedPtrListImpl &impl, bool deleteItems)
{
    clear(deleteItems);
    DeprecatedPtrListImpl(impl).swap(*this);
    return *this;
}

void DeprecatedPtrListImpl::addIterator(DeprecatedPtrListImplIterator *iter) const
{
    iter->next = iterators;
    iter->prev = 0;
    
    if (iterators) {
        iterators->prev = iter;
    }
    iterators = iter;
}

void DeprecatedPtrListImpl::removeIterator(DeprecatedPtrListImplIterator *iter) const
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

void DeprecatedPtrListImpl::swap(DeprecatedPtrListImpl &other)
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


DeprecatedPtrListImplIterator::DeprecatedPtrListImplIterator() :
    list(0),
    node(0)
{
}

DeprecatedPtrListImplIterator::DeprecatedPtrListImplIterator(const DeprecatedPtrListImpl &impl)  :
    list(&impl),
    node(impl.head)
{
    impl.addIterator(this);
}

DeprecatedPtrListImplIterator::~DeprecatedPtrListImplIterator()
{
    if (list) {
        list->removeIterator(this);
    }
}

DeprecatedPtrListImplIterator::DeprecatedPtrListImplIterator(const DeprecatedPtrListImplIterator &impl) :
    list(impl.list),
    node(impl.node)
{
    if (list) {
        list->addIterator(this);
    }
}

unsigned DeprecatedPtrListImplIterator::count() const
{
    return list == 0 ? 0 : list->count();
}

void *DeprecatedPtrListImplIterator::toFirst()
{
    if (list) {
        node = list->head;
    }
    return current();
}

void *DeprecatedPtrListImplIterator::toLast()
{
    if (list) {
        node = list->tail;
    }
    return current();
}

void *DeprecatedPtrListImplIterator::current() const
{
    return node == 0 ? 0 : node->data;
}

void *DeprecatedPtrListImplIterator::operator--()
{
    if (node) {
        node = node->prev;
    }
    return current();
}

void *DeprecatedPtrListImplIterator::operator++()
{
    if (node) {
        node = node->next;
    }
    return current();
}

DeprecatedPtrListImplIterator &DeprecatedPtrListImplIterator::operator=(const DeprecatedPtrListImplIterator &impl)
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

}
