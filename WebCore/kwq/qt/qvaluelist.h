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

#ifndef QVALUELIST_H_
#define QVALUELIST_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QVALUELIST ===================================================

#ifdef USING_BORROWED_QVALUELIST
#include <_qvaluelist.h>
#else

#include <KWQDef.h>
#include <KWQValueListImpl.h>

#include <iostream>

template <class T> class QValueList;
template <class T> class QValueListConstIterator;

template<class T> class QValueListNode : private KWQValueListNodeImpl {
public:
    QValueListNode(const T &val) : KWQValueListNodeImpl(), value(val) {}
    ~QValueListNode() {}

    T value;
    friend class QValueList<T>;
};


// class QValueListIterator ====================================================
template<class T> class QValueListIterator {
public: 
    QValueListIterator() : impl() {}
    QValueListIterator(const QValueListIterator<T> &other) : impl(other.impl) {}
     
    ~QValueListIterator() {}

    QValueListIterator &operator=(const QValueListIterator &other) { impl = other.impl; return *this; }

    bool operator==(const QValueListIterator<T> &other) { return impl == other.impl; }
    bool operator!=(const QValueListIterator<T> &other) { return impl != other.impl; }
    const T& operator*() const { return ((const QValueListNode<T> *)impl.node())->value; } 
    QValueListIterator operator++() { ++impl; return *this; }
    
    T& operator*() { return ((QValueListNode<T> *)impl.node())->value; } 
    QValueListIterator<T>& operator--() { --impl; return *this; }
    QValueListIterator operator++(int) { return QValueListIterator<T>(impl++); }

private:
    QValueListIterator(const KWQValueListIteratorImpl &pImp) : impl(pImp) {}

    KWQValueListIteratorImpl impl;

    friend class QValueList<T>;
    friend class QValueListConstIterator<T>;
}; // class QValueListIterator =================================================


// class QValueListConstIterator ===============================================
template<class T> class QValueListConstIterator {
public:
    QValueListConstIterator() : impl() {}
    QValueListConstIterator(const QValueListConstIterator<T> &other) : impl(other.impl) {}
    QValueListConstIterator(const QValueListIterator<T> &other) : impl(other.impl) {}
     
    ~QValueListConstIterator() {}

    QValueListConstIterator &operator=(const QValueListConstIterator &other) { impl = other.impl; return *this; }
    bool operator==(const QValueListConstIterator<T> &other) { return impl == other.impl; }
    bool operator!=(const QValueListConstIterator<T> &other) { return impl != other.impl; }
    const T& operator*() const { return ((const QValueListNode<T> *)impl.node())->value; } 
    QValueListConstIterator operator++() { ++impl; return *this; }
    QValueListConstIterator operator++(int) { return QValueListConstIterator<T>(impl++); }

private:
    QValueListConstIterator(const KWQValueListIteratorImpl &pImp) : impl(pImp) {}

    KWQValueListIteratorImpl impl;

    friend class QValueList<T>;
}; // class QValueListConstIterator ============================================


// class QValueList ============================================================

template <class T> class QValueList {
public:

    typedef QValueListIterator<T> Iterator;
    typedef QValueListConstIterator<T> ConstIterator;

    // constructors, copy constructors, and destructors ------------------------
    
    QValueList() : impl(deleteNode, copyNode) {}
    QValueList(const QValueList<T>&other) : impl(other.impl) {}
    
    ~QValueList() {}
        
    // member functions --------------------------------------------------------

    void clear() { impl.clear(); }
    uint count() const { return impl.count(); }
    bool isEmpty() const { return impl.isEmpty(); }

    void append(const T &val) { impl.appendNode(new QValueListNode<T>(val)); } 
    void prepend(const T &val) { impl.prependNode(new QValueListNode<T>(val)); } 
    void remove(const T &val) { QValueListNode<T> node(val); impl.removeEqualNodes(&node, nodesEqual); }
    uint contains(const T &val) const { QValueListNode<T> node(val); return impl.containsEqualNodes(&node, nodesEqual); }

    Iterator remove(Iterator iter) { return QValueListIterator<T>(impl.removeIterator(iter.impl)); }
    Iterator fromLast() { return QValueListIterator<T>(impl.fromLast()); }

    const T& first() const { return ((QValueListNode<T> *)impl.firstNode())->value; }
    const T& last() const { return ((QValueListNode<T> *)impl.lastNode())->value; }

    Iterator begin() { return QValueListIterator<T>(impl.begin()); }
    Iterator end() { return QValueListIterator<T>(impl.end()); }

    ConstIterator begin() const { return QValueListConstIterator<T>(impl.begin()); }
    ConstIterator end() const  { return QValueListConstIterator<T>(impl.end()); }

    // operators ---------------------------------------------------------------

    QValueList<T>& operator=(const QValueList<T>&other) { impl = other.impl; return *this; }
    T& operator[] (uint index) { return ((QValueListNode<T> *)impl.nodeAt(index))->value; }
    const T& operator[] (uint index) const { return ((const QValueListNode<T> *)impl.nodeAt(index))->value; }
    QValueList<T> &operator+=(const T &value) { impl.appendNode(new QValueListNode<T>(value)); return *this; } 

    QValueList<T> &operator<<(const T &value) { impl.appendNode(new QValueListNode<T>(value)); return *this; } 

private:
    KWQValueListImpl impl;

    static void deleteNode(KWQValueListNodeImpl *node) { delete (QValueListNode<T> *)node; }
    static bool nodesEqual(const KWQValueListNodeImpl *a, const KWQValueListNodeImpl *b) { return ((QValueListNode<T> *)a)->value == ((QValueListNode<T> *)b)->value; }	
    static KWQValueListNodeImpl *copyNode(KWQValueListNodeImpl *node) { return new QValueListNode<T>(((QValueListNode<T> *)node)->value); }
}; // class QValueList =========================================================


template<class T>
inline std::ostream &operator<<(std::ostream &o, const QValueList<T>&p)
{
    o <<
        "QValueList: [size: " <<
        (Q_UINT32)p.count() <<
        "; items: ";
        QValueListConstIterator<T> it = p.begin();
        while (it != p.end()) {
            o << *it;
            if (++it != p.end()) {
                o << ", ";
            }
        }
        o << "]";
    return o;
}


#endif // USING_BORROWED_QVALUELIST

#endif
