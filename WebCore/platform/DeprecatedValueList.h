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

#include "DeprecatedValueListImpl.h"

template <class T> class DeprecatedValueList;
template <class T> class DeprecatedValueListConstIterator;

template<class T> class DeprecatedValueListNode : private DeprecatedValueListImplNode {
public:
    DeprecatedValueListNode(const T &val) : value(val) { }
    T value;
    friend class DeprecatedValueList<T>;
};

template<class T> class DeprecatedValueListIterator {
public: 
    DeprecatedValueListIterator() { }

    T& operator*() const { return ((DeprecatedValueListNode<T> *)impl.node())->value; } 

    DeprecatedValueListIterator &operator++() { ++impl; return *this; }    
    DeprecatedValueListIterator &operator--() { --impl; return *this; }
    DeprecatedValueListIterator operator++(int) { return impl++; }

    bool operator==(const DeprecatedValueListIterator &other) { return impl == other.impl; }
    bool operator!=(const DeprecatedValueListIterator &other) { return impl != other.impl; }

private:
    DeprecatedValueListIterator(const DeprecatedValueListImplIterator &pImp) : impl(pImp) { }

    DeprecatedValueListImplIterator impl;

    friend class DeprecatedValueList<T>;
    friend class DeprecatedValueListConstIterator<T>;
};

template<class T> class DeprecatedValueListConstIterator {
public:
    DeprecatedValueListConstIterator() { }
    DeprecatedValueListConstIterator(const DeprecatedValueListIterator<T> &it) : impl(it.impl) { }

    const T& operator*() const { return ((const DeprecatedValueListNode<T> *)impl.node())->value; }
    
    DeprecatedValueListConstIterator &operator++() { ++impl; return *this; }
    DeprecatedValueListConstIterator &operator--() { --impl; return *this; }
    DeprecatedValueListConstIterator operator++(int) { return impl++; }

    bool operator==(const DeprecatedValueListConstIterator &other) { return impl == other.impl; }
    bool operator!=(const DeprecatedValueListConstIterator &other) { return impl != other.impl; }

private:
    DeprecatedValueListConstIterator(const DeprecatedValueListImplIterator &pImp) : impl(pImp) { }

    DeprecatedValueListImplIterator impl;

    friend class DeprecatedValueList<T>;
};

template<class T> bool operator==(const DeprecatedValueList<T> &a, const DeprecatedValueList<T> &b);

template <class T> class DeprecatedValueList {
public:
    typedef DeprecatedValueListIterator<T> Iterator;
    typedef DeprecatedValueListIterator<T> iterator;
    typedef DeprecatedValueListConstIterator<T> ConstIterator;
    typedef DeprecatedValueListConstIterator<T> const_iterator;

    DeprecatedValueList() : impl(deleteNode, copyNode) { }
        
    void clear() { impl.clear(); }
    unsigned count() const { return impl.count(); }
    bool isEmpty() const { return impl.isEmpty(); }

    Iterator append(const T &val) { return impl.appendNode(new DeprecatedValueListNode<T>(val)); } 
    Iterator prepend(const T &val) { return impl.prependNode(new DeprecatedValueListNode<T>(val)); } 
    void remove(const T &val) { DeprecatedValueListNode<T> node(val); impl.removeEqualNodes(&node, nodesEqual); }
    unsigned contains(const T &val) const { DeprecatedValueListNode<T> node(val); return impl.containsEqualNodes(&node, nodesEqual); }
    Iterator find(const T &val) const { DeprecatedValueListNode<T> node(val); return impl.findEqualNode(&node, nodesEqual); }

    Iterator insert(Iterator iter, const T& val) { return impl.insert(iter.impl, new DeprecatedValueListNode<T>(val)); }
    Iterator remove(Iterator iter) { return impl.removeIterator(iter.impl); }
    Iterator fromLast() { return impl.fromLast(); }

    T& first() { return static_cast<DeprecatedValueListNode<T> *>(impl.firstNode())->value; }
    const T& first() const { return static_cast<DeprecatedValueListNode<T> *>(impl.firstNode())->value; }
    T& last() { return static_cast<DeprecatedValueListNode<T> *>(impl.lastNode())->value; }
    const T& last() const { return static_cast<DeprecatedValueListNode<T> *>(impl.lastNode())->value; }

    Iterator begin() { return impl.begin(); }
    Iterator end() { return impl.end(); }

    ConstIterator begin() const { return impl.begin(); }
    ConstIterator end() const { return impl.end(); }
    ConstIterator constBegin() const { return impl.begin(); }
    ConstIterator constEnd() const { return impl.end(); }
    ConstIterator fromLast() const { return impl.fromLast(); }

    T& operator[] (unsigned index) { return ((DeprecatedValueListNode<T> *)impl.nodeAt(index))->value; }
    const T& operator[] (unsigned index) const { return ((const DeprecatedValueListNode<T> *)impl.nodeAt(index))->value; }
    DeprecatedValueList &operator+=(const T &value) { impl.appendNode(new DeprecatedValueListNode<T>(value)); return *this; }
    DeprecatedValueList &operator<<(const T &value) { impl.appendNode(new DeprecatedValueListNode<T>(value)); return *this; }
    
    friend bool operator==<>(const DeprecatedValueList<T> &, const DeprecatedValueList<T> &);
    
private:
    DeprecatedValueListImpl impl;

    static void deleteNode(DeprecatedValueListImplNode *node) { delete (DeprecatedValueListNode<T> *)node; }
    static bool nodesEqual(const DeprecatedValueListImplNode *a, const DeprecatedValueListImplNode *b)
        { return ((DeprecatedValueListNode<T> *)a)->value == ((DeprecatedValueListNode<T> *)b)->value; }
    static DeprecatedValueListImplNode *copyNode(DeprecatedValueListImplNode *node)
        { return new DeprecatedValueListNode<T>(((DeprecatedValueListNode<T> *)node)->value); }
};

template<class T>
inline bool operator==(const DeprecatedValueList<T> &a, const DeprecatedValueList<T> &b)
{
    return a.impl.isEqual(b.impl, DeprecatedValueList<T>::nodesEqual);
}

#endif
