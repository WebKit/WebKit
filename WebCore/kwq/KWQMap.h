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

#ifndef QMAP_H_
#define QMAP_H_

#include "KWQDef.h"

#include "KWQMapImpl.h"

template <class K, class V> class QMap;
template <class K, class V> class QMapIterator;
template <class K, class V> class QMapConstIterator;

template <class K, class V> class QMapNode : private KWQMapNodeImpl
{
 private:
    QMapNode(K k, V v) : 
	key(k),
	value(v)
    {
    }
    
    // intentionally not defined
    QMapNode(const QMapNode&node);
    QMapNode& operator=(const QMapNode&);

    ~QMapNode()
    {
	delete (QMapNode *)left();
	delete (QMapNode *)right();
    }

    K key;
    V value;

    friend class QMap<K,V>;
    friend class QMapIterator<K,V>;
    friend class QMapConstIterator<K,V>;
};

template<class K, class V> class QMapIterator : private KWQMapIteratorImpl {
public:
    QMapIterator() { }
    
    const K& key() const
    {
	return ((QMapNode<K,V> *)node)->key;
    }

    const V& data() const
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    bool operator==(const QMapIterator<K,V> &iter) const
    {
	return node == iter.node;
    }

    bool operator!=(const QMapIterator<K,V> &iter) const
    {
	return node != iter.node;
    }

    V& operator*()
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    const V& operator*() const
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    QMapIterator<K,V>& operator++()
    {
	incrementInternal();
	return *this;
    }

private:
    QMapIterator(QMapNode<K,V> *n)
    {
        node = n;
    }

    friend class QMap<K,V>;
    friend class QMapConstIterator<K,V>;
};

template<class K, class V> class QMapConstIterator : private KWQMapIteratorImpl {
public:
    QMapConstIterator() { }
    QMapConstIterator(const QMapIterator<K,V> &iter) : KWQMapIteratorImpl(iter) { }

    const K& key() const
    {
	return ((QMapNode<K,V> *)node)->key;
    }

    const V& data() const
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    bool operator==(const QMapConstIterator<K,V> &citer) const
    {
	return node == citer.node;
    }

    bool operator!=(const QMapConstIterator<K,V> &citer) const
    {
	return node != citer.node;
    }

    const V &operator*() const
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    QMapConstIterator<K,V>& operator++()
    {
	incrementInternal();
	return *this;
    }

private:
    QMapConstIterator(const QMapNode<K,V> *n)
    {
	node = (KWQMapNodeImpl *)n;
    }

    friend class QMap<K,V>;
};

template <class K, class V> class QMap : public KWQMapImpl {
public:
    typedef QMapIterator<K,V> Iterator;
    typedef QMapConstIterator<K,V> ConstIterator;

    QMap() : 
	KWQMapImpl(new QMapNode<K,V>(K(),V()), deleteNode)
    {
    }

    QMap(const QMap<K,V>& m) :
	KWQMapImpl(m)
    {
    }

    void clear() { clearInternal(); }

    uint count() const { return countInternal(); }

    Iterator begin() { return (QMapNode<K,V> *)beginInternal(); }
    Iterator end() { return (QMapNode<K,V> *)endInternal(); }
    ConstIterator begin() const { return (QMapNode<K,V> *)beginInternal(); }
    ConstIterator end() const { return ConstIterator((QMapNode<K,V> *)endInternal()); }

    Iterator insert(const K& key, const V& value)
    {
	QMapNode<K,V> tmp(key,value);
	return Iterator((QMapNode<K,V> *)insertInternal(&tmp, true));
    }

    void remove(const K& key)
    {
	QMapNode<K,V> tmp(key, V());
	removeEqualInternal(&tmp);
    }

    void remove(const QMapIterator<K,V> &iterator)
    {
	removeEqualInternal(iterator.node, true);
    }

    Iterator find (const K &key)
    {
	QMapNode<K,V> tmp(key, V());
	QMapNode<K,V> *result = (QMapNode<K,V> *)findInternal(&tmp);
	
	if (result != NULL) {
	    return Iterator(result);
	} else {
	    return Iterator(end());
	}
    }

    ConstIterator find (const K &key) const
    {
	QMapNode<K,V> tmp(key, V());
	QMapNode<K,V> *result = (QMapNode<K,V> *)findInternal(&tmp);
	
	if (result != NULL) {
	    return ConstIterator(result);
	} else {
	    return ConstIterator(end());
	}
    }
    
    bool contains(const K &key) const
    {
	QMapNode<K,V> tmp(key, V());
	return findInternal(&tmp);
    }

    QMap<K,V>& operator=(const QMap<K,V>&map)
    {
	QMap<K,V> tmp(map);
	swap(tmp);
	return *this;
    }

    V& operator[](const K& key)
    {
	QMapNode<K,V> tmp(key, V());
	return ((QMapNode<K,V> *)insertInternal(&tmp, false))->value;
    }

    V operator[](const K& key) const
    {
	QMapNode<K,V> tmp(key, V());
        QMapNode<K,V> *result = (QMapNode<K,V> *)findInternal(&tmp);
	return result ? result->value : V();
    }

protected:
    virtual void copyNode(const KWQMapNodeImpl *isrc, KWQMapNodeImpl *idst) const
    {
 	QMapNode<K,V> *src = (QMapNode<K,V> *)isrc;
 	QMapNode<K,V> *dst = (QMapNode<K,V> *)idst;
	dst->key = src->key;
	dst->value = src->value;
    }
    
    virtual KWQMapNodeImpl *duplicateNode(const KWQMapNodeImpl *isrc) const
    {
 	QMapNode<K,V> *src = (QMapNode<K,V> *)isrc;
	return new QMapNode<K,V>(src->key, src->value);
    }

    virtual CompareResult compareNodes(const KWQMapNodeImpl *ia, const KWQMapNodeImpl *ib) const
    {
 	QMapNode<K,V> *a = (QMapNode<K,V> *)ia;
 	QMapNode<K,V> *b = (QMapNode<K,V> *)ib;

	if (a->key == b->key) {
	    return Equal;
	} else if (a->key < b->key) {
	    return Less;
	} else {
	    return Greater;
	}
    }

    virtual void swapNodes(KWQMapNodeImpl *ia, KWQMapNodeImpl *ib) const
    {
	QMapNode<K,V> *a = (QMapNode<K,V> *)ia;
	QMapNode<K,V> *b = (QMapNode<K,V> *)ib;

	K tmpKey = a->key;
	V tmpValue = a->value;
	a->key = b->key;
	a->value = b->value;
	b->key = tmpKey;
	b->value = tmpValue;
    }

    static void deleteNode(KWQMapNodeImpl *inode)
    {
	delete (QMapNode<K,V> *)inode;
    }

};

#endif
