/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef QVALUELIST_H_
#define QVALUELIST_H_

#include "KWQDef.h"
#include "KWQValueListImpl.h"

template <class T> class QValueList;
template <class T> class QValueListConstIterator;

template<class T> class QValueListNode : private KWQValueListNodeImpl {
public:
    QValueListNode(const T &val) : value(val) { }
    T value;
    friend class QValueList<T>;
};

template<class T> class QValueListIterator {
public: 
    QValueListIterator() { }

    T& operator*() const { return ((QValueListNode<T> *)impl.node())->value; } 

    QValueListIterator &operator++() { ++impl; return *this; }    
    QValueListIterator &operator--() { --impl; return *this; }
    QValueListIterator operator++(int) { return impl++; }

    bool operator==(const QValueListIterator &other) { return impl == other.impl; }
    bool operator!=(const QValueListIterator &other) { return impl != other.impl; }

private:
    QValueListIterator(const KWQValueListIteratorImpl &pImp) : impl(pImp) { }

    KWQValueListIteratorImpl impl;

    friend class QValueList<T>;
    friend class QValueListConstIterator<T>;
};

template<class T> class QValueListConstIterator {
public:
    QValueListConstIterator() { }
    QValueListConstIterator(const QValueListIterator<T> &it) : impl(it.impl) { }

    const T& operator*() const { return ((const QValueListNode<T> *)impl.node())->value; }
    
    QValueListConstIterator &operator++() { ++impl; return *this; }
    QValueListConstIterator &operator--() { --impl; return *this; }
    QValueListConstIterator operator++(int) { return impl++; }

    bool operator==(const QValueListConstIterator &other) { return impl == other.impl; }
    bool operator!=(const QValueListConstIterator &other) { return impl != other.impl; }

private:
    QValueListConstIterator(const KWQValueListIteratorImpl &pImp) : impl(pImp) { }

    KWQValueListIteratorImpl impl;

    friend class QValueList<T>;
};

template<class T> bool operator==(const QValueList<T> &a, const QValueList<T> &b);

template <class T> class QValueList {
public:
    typedef QValueListIterator<T> Iterator;
    typedef QValueListIterator<T> iterator;
    typedef QValueListConstIterator<T> ConstIterator;
    typedef QValueListConstIterator<T> const_iterator;

    QValueList() : impl(deleteNode, copyNode) { }
        
    void clear() { impl.clear(); }
    uint count() const { return impl.count(); }
    bool isEmpty() const { return impl.isEmpty(); }

    Iterator append(const T &val) { return impl.appendNode(new QValueListNode<T>(val)); } 
    Iterator prepend(const T &val) { return impl.prependNode(new QValueListNode<T>(val)); } 
    void remove(const T &val) { QValueListNode<T> node(val); impl.removeEqualNodes(&node, nodesEqual); }
    uint contains(const T &val) const { QValueListNode<T> node(val); return impl.containsEqualNodes(&node, nodesEqual); }
    Iterator find(const T &val) const { QValueListNode<T> node(val); return impl.findEqualNode(&node, nodesEqual); }

    Iterator insert(Iterator iter, const T& val) { return impl.insert(iter.impl, new QValueListNode<T>(val)); }
    Iterator remove(Iterator iter) { return impl.removeIterator(iter.impl); }
    Iterator fromLast() { return impl.fromLast(); }

    T& first() { return static_cast<QValueListNode<T> *>(impl.firstNode())->value; }
    const T& first() const { return static_cast<QValueListNode<T> *>(impl.firstNode())->value; }
    T& last() { return static_cast<QValueListNode<T> *>(impl.lastNode())->value; }
    const T& last() const { return static_cast<QValueListNode<T> *>(impl.lastNode())->value; }

    Iterator begin() { return impl.begin(); }
    Iterator end() { return impl.end(); }

    ConstIterator begin() const { return impl.begin(); }
    ConstIterator end() const { return impl.end(); }
    ConstIterator constBegin() const { return impl.begin(); }
    ConstIterator constEnd() const { return impl.end(); }
    ConstIterator fromLast() const { return impl.fromLast(); }

    T& operator[] (uint index) { return ((QValueListNode<T> *)impl.nodeAt(index))->value; }
    const T& operator[] (uint index) const { return ((const QValueListNode<T> *)impl.nodeAt(index))->value; }
    QValueList &operator+=(const T &value) { impl.appendNode(new QValueListNode<T>(value)); return *this; }
    QValueList &operator<<(const T &value) { impl.appendNode(new QValueListNode<T>(value)); return *this; }
    
    friend bool operator==<>(const QValueList<T> &, const QValueList<T> &);
    
private:
    KWQValueListImpl impl;

    static void deleteNode(KWQValueListNodeImpl *node) { delete (QValueListNode<T> *)node; }
    static bool nodesEqual(const KWQValueListNodeImpl *a, const KWQValueListNodeImpl *b)
        { return ((QValueListNode<T> *)a)->value == ((QValueListNode<T> *)b)->value; }
    static KWQValueListNodeImpl *copyNode(KWQValueListNodeImpl *node)
        { return new QValueListNode<T>(((QValueListNode<T> *)node)->value); }
};

template<class T>
inline bool operator==(const QValueList<T> &a, const QValueList<T> &b)
{
    return a.impl.isEqual(b.impl, QValueList<T>::nodesEqual);
}

#define Q3ValueList QValueList
#define Q3ValueListIterator QValueListIterator
#define Q3ValueListConstIterator QValueListConstIterator

#endif
