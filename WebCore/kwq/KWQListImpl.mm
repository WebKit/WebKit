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

#include <cstddef>
#include <CoreFoundation/CFArray.h>

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
    delete next;
}

// KWQListImpl::KWQListPrivate

class KWQListImpl::KWQListPrivate
{
public:
    KWQListPrivate(void (*deleteFunc)(void *));
    KWQListPrivate(KWQListPrivate &vp);
    ~KWQListPrivate();
    
    static KWQListNode * copyList(KWQListNode *, KWQListNode *&tail);

    KWQListNode *head;
    KWQListNode *tail;
    KWQListNode *current;
    uint count;
    void (*deleteItem)(void *);
    KWQListNode *iterators;
};

KWQListNode *KWQListImpl::KWQListPrivate::copyList(KWQListNode *l, KWQListNode *&tail)
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
	
	last = copy;
	node = node->next;
    }

    tail = last;
    return copyHead;
}

KWQListImpl::KWQListPrivate::KWQListPrivate(void (*deleteFunc)(void *)) :
    head(NULL),
    tail(NULL),
    current(NULL),
    count(0),
    deleteItem(deleteFunc),
    iterators(NULL)
{
}

KWQListImpl::KWQListPrivate::KWQListPrivate(KWQListPrivate &vp) :
    current(NULL),
    count(vp.count),
    deleteItem(vp.deleteItem),
    iterators(NULL)
{
    head = copyList(vp.head, tail);
}

KWQListImpl::KWQListPrivate::~KWQListPrivate()
{
    delete head;
    delete iterators;
}


// KWQListIteratorImpl::KWQListIteratorPrivate

class KWQListIteratorImpl::KWQListIteratorPrivate
{
public:
    KWQListIteratorPrivate();
    KWQListIteratorPrivate(const KWQListImpl *list, KWQListNode *n);

    const KWQListImpl *list;
    KWQListNode *node;
};

KWQListIteratorImpl::KWQListIteratorPrivate::KWQListIteratorPrivate() :
    list(NULL),
    node(NULL)
{
}

KWQListIteratorImpl::KWQListIteratorPrivate::KWQListIteratorPrivate(const KWQListImpl *l, KWQListNode *n) :
    list(l),
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
    for (KWQListNode *node = d->iterators; node != NULL; node = node->next) {
	KWQListIteratorImpl::KWQListIteratorPrivate *p = ((KWQListIteratorImpl *)node->data)->d;
	p->list = NULL;
        p->node = NULL;
    }
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
    d->tail = NULL;
    d->current = NULL;
    d->count = 0;
}

void KWQListImpl::sort(int (*compareFunc)(void *a, void *b, void *data), void *data)
{
    // no sorting for 0 or 1-element lists
    if (d->count <= 1) {
        return;
    }

    // special case for 2-element lists
    KWQListNode *head = d->head;
    if (d->count == 2) {
        void *a = head->data;
        void *b = head->next->data;
        if (compareFunc(a, b, data) <= 0) {
            return;
        }
        head->next->data = a;
        head->data = b;
        return;
    }

    // insertion sort for most common sizes
    const uint cutoff = 32;
    if (d->count <= cutoff) {
        // Straight out of Sedgewick's Algorithms in C++.

        // put all the elements into an array
        void *a[cutoff];
        uint i = 0;
        for (KWQListNode *node = head; node != NULL; node = node->next) {
            a[i++] = node->data;
        }

        // move the smallest element to the start to serve as a sentinel
        for (i = d->count - 1; i > 0; i--) {
            if (compareFunc(a[i-1], a[i], data) > 0) {
                void *t = a[i-1];
                a[i-1] = a[i];
                a[i] = t;
            }
        }

        // move other items to the right and put a[i] into position
        for (i = 2; i < d->count; i++) {
            void *v = a[i];
            uint j = i;
            while (compareFunc(v, a[j-1], data) < 0) {
                a[j] = a[j-1];
                j--;
            }
            a[j] = v;
        }

        // finally, put everything back into the list
        i = 0;
        for (KWQListNode *node = head; node != NULL; node = node->next) {
            node->data = a[i++];
        }
        return;
    }

    // CFArray sort for larger lists
    
    CFMutableArrayRef array = CFArrayCreateMutable(NULL, d->count, NULL);

    for (KWQListNode *node = head; node != NULL; node = node->next) {
	CFArrayAppendValue(array, node->data);
    }

    CFArraySortValues(array, CFRangeMake(0, d->count), (CFComparatorFunction) compareFunc, data);

    int i = 0;
    for (KWQListNode *node = head; node != NULL; node = node->next) {
	node->data = const_cast<void *>(CFArrayGetValueAtIndex(array, i++));
    }

    CFRelease(array);
}

void *KWQListImpl::at(uint n)
{
    KWQListNode *node;
    if (n >= d->count - 1) {
        node = d->tail;
    } else {
        node = d->head;
        for (uint i = 0; i < n && node != NULL; i++) {
            node = node->next;
        }
    }

    d->current = node;
    return node->data;
}

bool KWQListImpl::insert(uint n, const void *item)
{
    if (n > d->count) {
	return false;
    }

    KWQListNode *node = new KWQListNode;
    node->data = (void *)item;

    if (n == 0) {
	// inserting at head
	node->next = d->head;
	if (d->head != NULL) {
	    d->head->prev = node;
	}
	d->head = node;
        if (d->tail == NULL) {
            d->tail = node;
        }
    } else if (n == d->count) {
        // inserting at tail
        node->prev = d->tail;
        if (d->tail != NULL) {
            d->tail->next = node;
        }
        d->tail = node;
    } else {
	// general insertion
	
	// iterate to one node before the insertion point, can't be null
	// since we know n > 0 and n < d->count
	KWQListNode *prevNode = d->head;

	for (uint i = 0; i < n - 1; i++) {
	    prevNode = prevNode->next;
	}
	node->prev = prevNode;
	node->next = prevNode->next;
	if (node->next != NULL) {
	    node->next->prev = node;
	}
	prevNode->next = node;
    }

    d->count++;
    d->current = node;
    return true;
}

bool KWQListImpl::remove(bool deleteItem)
{
    KWQListNode *node = d->current;
    if (node == NULL) {
	return false;
    }

    if (node->prev == NULL) {
	d->head = node->next;
    } else {
	node->prev->next = node->next;
    }

    if (node->next == NULL) {
        d->tail = node->prev;
    } else {
	node->next->prev = node->prev;
    }

    if (node->next != NULL) {
	d->current = node->next;
    } else {
	d->current = node->prev;
    }

    if (deleteItem) {
	d->deleteItem(node->data);
    }

    for (KWQListNode *iterator = d->iterators; iterator != NULL; iterator = iterator->next) {
	if (((KWQListIteratorImpl *)iterator->data)->d->node == node) {
	    ((KWQListIteratorImpl *)iterator->data)->d->node = d->current;
	}
    }

    node->next = NULL;
    delete node;
    d->count--;

    return true;
}

bool KWQListImpl::remove(uint n, bool deleteItem)
{
    if (n >= d->count) {
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

void *KWQListImpl::getFirst() const
{
    return d->head ? d->head->data : 0;
}

void *KWQListImpl::getLast() const
{
    return d->tail ? d->tail->data : 0;
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
    d->current = d->tail;
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

void *KWQListImpl::take()
{
    void *retval = current();
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
    // FIXME: this doesn't respect the deleteItems flag
    
    KWQListImpl tmp(impl);
    KWQListImpl::KWQListPrivate *tmpD = tmp.d;

    tmp.d = d;
    d = tmpD;

    return *this;
}

void KWQListImpl::addIterator(KWQListIteratorImpl *iter) const
{
    KWQListNode *node = new KWQListNode;
    node->data = iter;
    node->next = d->iterators;
    if (node->next != NULL) {
        node->next->prev = node;
    }
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
    d(new KWQListIteratorImpl::KWQListIteratorPrivate(&impl, impl.d->head))
{
    impl.addIterator(this);
}

KWQListIteratorImpl::~KWQListIteratorImpl()
{
    if (d->list != NULL) {
	d->list->removeIterator(this);
    }
    delete d;
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListIteratorImpl &impl) :
    d(new KWQListIteratorImpl::KWQListIteratorPrivate(impl.d->list, impl.d->node))
{
    if (d->list != NULL) {
        d->list->addIterator(this);
    }
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
    d->node = d->list->d->tail;
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
    if (d->list != NULL) {
	d->list->removeIterator(this);
    }
    
    d->list = impl.d->list;
    d->node = impl.d->node;
    
    if (d->list != NULL) {
	d->list->addIterator(this);
    }

    return *this;
}
