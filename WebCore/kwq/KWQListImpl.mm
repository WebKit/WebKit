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

#include <KWQListImpl.h>

#ifndef USING_BORROWED_QLIST

#include <cstddef>

#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#include <CoreFoundation/CFArray.h>
#undef Fixed
#undef Rect
#undef Boolean

// KWQListNode

class KWQListNode
{
public:
    KWQListNode() : data(NULL), next(NULL), prev(NULL) {}
    ~KWQListNode();

    void *data;
    KWQListNode *next;
    KWQListNode *prev;
};


KWQListNode::~KWQListNode()
{
    if (next != NULL) {
	delete next;
    }
}

// KWQListImpl::KWQListPrivate

class KWQListImpl::KWQListPrivate
{
public:
    KWQListPrivate(void (*deleteFunc)(void *));
    KWQListPrivate(KWQListPrivate &vp);
    ~KWQListPrivate();
    
    static KWQListNode *copyList(KWQListNode *l);

    KWQListNode *head;
    // KWQListNode *tail;
    KWQListNode *current;
    uint count;
    void (*deleteItem)(void *);
    KWQListNode *iterators;
};

KWQListNode *KWQListImpl::KWQListPrivate::copyList(KWQListNode *l)
{
    KWQListNode *node = l;
    KWQListNode *copyHead = NULL;
    KWQListNode *last = NULL;

    while (node != NULL) {
	KWQListNode *copy = new KWQListNode;
	copy->data = node->data;
	if (last != NULL) {
	    last->next = copy;
	} else {
	    copyHead = copy;
	}

	copy->prev = last;
	copy->next = NULL;
	
	last = copy;
	node = node->next;
    }

    return copyHead;
}

KWQListImpl::KWQListPrivate::KWQListPrivate(void (*deleteFunc)(void *)) :
    head(NULL),
    current(NULL),
    count(0),
    deleteItem(deleteFunc),
    iterators(NULL)
{
}

KWQListImpl::KWQListPrivate::KWQListPrivate(KWQListPrivate &vp) :
    head(copyList(vp.head)),
    current(NULL),
    count(vp.count),
    deleteItem(vp.deleteItem),
    iterators(NULL)
{
}

KWQListImpl::KWQListPrivate::~KWQListPrivate()
{
    delete head;
}

// KWQListIteratorImpl::KWQListIteratorPrivate

class KWQListIteratorImpl::KWQListIteratorPrivate
{
public:
    KWQListIteratorPrivate();
    KWQListIteratorPrivate(const KWQListImpl &list, KWQListNode *n);

    const KWQListImpl *list;
    KWQListNode *node;
};

KWQListIteratorImpl::KWQListIteratorPrivate::KWQListIteratorPrivate() :
    list(NULL),
    node(NULL)
{
}

KWQListIteratorImpl::KWQListIteratorPrivate::KWQListIteratorPrivate(const KWQListImpl &l, KWQListNode *n) :
    list(&l),
    node(n)
{
}



// KWQListImpl

KWQListImpl::KWQListImpl(void (*deleteFunc)(void *)) :
    d(new KWQListImpl::KWQListPrivate(deleteFunc))
{
}

KWQListImpl::KWQListImpl(const KWQListImpl &impl) :
    d(new KWQListImpl::KWQListPrivate(*impl.d))
{
}

KWQListImpl::~KWQListImpl()
{
    delete d;
}
     
bool KWQListImpl::isEmpty() const
{
    return d->count == 0;
}

uint KWQListImpl::count() const
{
    return d->count;
}

void KWQListImpl::clear(bool deleteItems)
{
    if (deleteItems) {
	KWQListNode *node = d->head;

	while (node != NULL) {
	    d->deleteItem(node->data);
	    node = node->next;
	}
    }

    delete d->head;
    d->head = NULL;
    d->current = NULL;
    d->count = 0;
}

void KWQListImpl::sort(int (*compareFunc)(void *a, void *b, void *data), void *data)
{
    CFMutableArrayRef array = CFArrayCreateMutable(NULL, d->count, NULL);

    for (KWQListNode *node = d->head; node != NULL; node = node->next) {
	CFArrayAppendValue(array, node->data);
    }

    CFArraySortValues(array, CFRangeMake(0, d->count), (CFComparatorFunction) compareFunc, data);

    int i = 0;
    for (KWQListNode *node = d->head; node != NULL; node = node->next) {
	node->data = (void *)CFArrayGetValueAtIndex(array, i);
	i++;
    }

    CFRelease(array);
}

void *KWQListImpl::at(uint n)
{
    d->current = d->head;
    uint i = 0; 

    while (i < n && d->current != NULL) {
	d->current = d->current->next;
	i++;
    }

    return d->current->data;
}

bool KWQListImpl::insert(uint n, const void *item)
{
    if (n > d->count) {
	return false;
    }

    d->current = new KWQListNode;
    d->current->data = (void *)item;

    if (n == 0) {
	// inserting at head
	d->current->next = d->head;
	if (d->head != NULL) {
	    d->head->prev = d->current;
	}
	d->head = d->current;
    } else {
	// general insertion
	
	// iterate to one node before the insertion point, can't be null
	// since we know n > 0 and n <= d->count
	KWQListNode *node = d->head;

	for  (unsigned i = 0; i < n - 1; i++) {
	    node = node->next;
	}
	d->current->prev = node;
	d->current->next = node->next;
	if (node->next != NULL) {
	    node->next->prev = d->current;
	}
	node->next = d->current;
    }

    d->count++;
    return true;
}

bool KWQListImpl::remove(bool deleteItem)
{
    if (d->current == NULL) {
	return false;
    }

    if (d->head == d->current) {
	d->head = d->current->next;
    } else {
	d->current->prev->next = d->current->next;
    }

    if (d->current->next != NULL) {
	d->current->next->prev = d->current->prev;
    }

    KWQListNode *detached = d->current;
    if (detached->next != NULL) {
	d->current = detached->next;
    } else {
	d->current = detached->prev;
    }

    detached->next = NULL;

    if (deleteItem) {
	d->deleteItem(detached->data);
    }

    for (KWQListNode *iterator = d->iterators; iterator != NULL; iterator = iterator->next) {
	if (((KWQListIteratorImpl *)iterator->data)->d->node == detached) {
	    ((KWQListIteratorImpl *)iterator->data)->d->node = d->current;
	}
    }
    
    delete detached;
    d->count--;

    return true;
}

bool KWQListImpl::remove(uint n, bool deleteItem)
{
    if (n >= d->count) {
	return false;
    }
    
    d->current = d->head;

    for (uint i = 0; i < n; i++) {
	d->current = d->current->next;
    }

    return remove(deleteItem);
}

bool KWQListImpl::removeFirst(bool deleteItem)
{
    return remove((unsigned)0, deleteItem);
}

bool KWQListImpl::removeLast(bool deleteItem)
{
    return remove(d->count - 1, deleteItem);
}

bool KWQListImpl::remove(const void *item, bool deleteItem, int (*compareFunc)(void *a, void *b, void *data), void *data)
{
    KWQListNode *node;

    node = d->head;

    while (node != NULL && compareFunc((void *)item, node->data, data) != 0) {
	node = node->next;
    }
    
    if (node == NULL) {
	return false;
    }

    d->current = node;

    return remove(deleteItem);
}

bool KWQListImpl::removeRef(const void *item, bool deleteItem)
{
    KWQListNode *node;

    node = d->head;

    while (node != NULL && item != node->data) {
	node = node->next;
    }
    
    if (node == NULL) {
	return false;
    }

    d->current = node;

    return remove(deleteItem);
}

void *KWQListImpl::getLast() const
{
    KWQListNode *node = d->head;

    if (node == NULL) {
	return NULL;
    } 
    
    while (node->next != NULL) {
	node = node->next;
    }
    return node->data;
}

void *KWQListImpl::current() const
{
    if (d->current != NULL) {
	return d->current->data;
    } else {
	return NULL;
    }
}

void *KWQListImpl::first()
{
    d->current = d->head;
    return current();
}

void *KWQListImpl::last()
{
    if (d->head != NULL) {
	d->current = d->head;
	while (d->current->next != NULL) {
	    d->current = d->current->next;
	}
    }
    return current();
}


void *KWQListImpl::next()
{
    if (d->current != NULL) {
	d->current = d->current->next;
    }
    return current();
}

void *KWQListImpl::prev()
{
    if (d->current != NULL) {
	d->current = d->current->prev;
    }
    return current();
}

void *KWQListImpl::take(uint n)
{
    void *retval = at(n);
    remove(false);
    return retval;
}

void KWQListImpl::append(const void *item)
{
    insert(d->count, item);
}

void KWQListImpl::prepend(const void *item)
{
    insert(0, item);
}

uint KWQListImpl::containsRef(const void *item) const
{
    KWQListNode *node = d->head;

    while (node != NULL && item != node->data) {
	node = node->next;
    }
    
    return node != NULL;
}

KWQListImpl &KWQListImpl::assign(const KWQListImpl &impl, bool deleteItems)
{
    KWQListImpl tmp(impl);
    KWQListImpl::KWQListPrivate *tmpD = tmp.d;

    tmp.d = d;
    d = tmpD;

    return *this;
}


void KWQListImpl::addIterator(KWQListIteratorImpl *iter) const
{
    KWQListNode *node = new KWQListNode();
    node->data = iter;
    node->next = d->iterators;
    d->iterators = node;
}

void KWQListImpl::removeIterator(KWQListIteratorImpl *iter) const
{
    KWQListNode *node = d->iterators;

    while (node != NULL && node->data != iter) {
	node = node->next;
    }

    if (node != NULL) {
 	if (node->prev == NULL) {
	    d->iterators = node->next;
	} else {
	    node->prev->next = node->next;
	}

 	if (node->next != NULL) {
	    node->next->prev = node->prev;
	}

	node->next = NULL;
	delete node;
    }
}




// KWQListIteratorImpl

KWQListIteratorImpl::KWQListIteratorImpl() :
    d(new KWQListIteratorImpl::KWQListIteratorPrivate())
{
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListImpl &impl)  :
    d(new KWQListIteratorImpl::KWQListIteratorPrivate(impl, impl.d->head))
{
    d->list->addIterator(this);
}

KWQListIteratorImpl::~KWQListIteratorImpl()
{
    if (d->list != NULL) {
	d->list->removeIterator(this);
    }
    delete d;
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListIteratorImpl &impl) :
    d(new KWQListIteratorImpl::KWQListIteratorPrivate(*impl.d->list, impl.d->node))
{
    d->list->addIterator(this);
}

uint KWQListIteratorImpl::count() const
{
    if (d->list == NULL) {
	return 0;
    } else {
	return d->list->count();
    }
}

void *KWQListIteratorImpl::toFirst()
{
    d->node = d->list->d->head;
    return current();
}

void *KWQListIteratorImpl::toLast()
{
    d->node = d->list->d->head;
    while (d->node != NULL && d->node->next != NULL) {
	d->node = d->node->next;
    }
    return current();
}

void *KWQListIteratorImpl::current() const
{
    if (d->node == NULL) {
	return NULL;
    } else {
	return d->node->data;
    }
}

void *KWQListIteratorImpl::operator--()
{
    if (d->node != NULL) {
	d->node = d->node->prev;
    }
    return current();
}

void *KWQListIteratorImpl::operator++()
{
    if (d->node != NULL) {
	d->node = d->node->next;
    }
    return current();
}

KWQListIteratorImpl &KWQListIteratorImpl::operator=(const KWQListIteratorImpl &impl)
{
    KWQListIteratorImpl tmp(impl);
    KWQListIteratorImpl::KWQListIteratorPrivate *tmpD = tmp.d;

    if (d->list != NULL) {
	d->list->removeIterator(this);
    }
    if (tmp.d->list != NULL) {
	tmp.d->list->removeIterator(&tmp);
    }

    tmp.d = d;
    d = tmpD;

    if (tmp.d->list != NULL) {
	tmp.d->list->addIterator(&tmp);
    }
    if (d->list != NULL) {
	d->list->addIterator(this);
    }

    return *this;
}

#endif
