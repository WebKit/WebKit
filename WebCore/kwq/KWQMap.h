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

#ifndef QMAP_H_
#define QMAP_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QMAP =========================================================

#ifdef USING_BORROWED_QMAP
#include <_qmap.h>
#else

#include <iostream>
#include <KWQDef.h>

#include <KWQMapImpl.h>

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
    QMapNode(const QMapNode<K,V>&node);

    ~QMapNode()
    {
	delete left();
	delete right();
    }

    K key;
    V value;

    friend class QMap<K,V>;
    friend class QMapIterator<K,V>;
    friend class QMapConstIterator<K,V>;
};

// class QMapIterator ==========================================================

template<class K, class V> class QMapIterator : private KWQMapIteratorImpl {
public:
    QMapIterator()
    {
    }

    QMapIterator(const QMapIterator<K,V> &iter) : KWQMapIteratorImpl(iter)
    {
    }

    ~QMapIterator()
    {
    }

    const K& key() const
    {
	return ((QMapNode<K,V> *)node)->key;
    }

    const V& data() const
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    QMapIterator<K,V> &operator=(const QMapIterator<K,V> &iter)
    {
	node = iter.node;
	return *this;
    }

    bool operator==(const QMapIterator<K,V>&iter) const
    {
	return node == iter.node;
    }

    bool operator!=(const QMapIterator<K,V>&iter) const
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
}; // class QMapIterator =======================================================


// class QMapConstIterator =====================================================

template<class K, class V> class QMapConstIterator : private KWQMapIteratorImpl {
public:
    QMapConstIterator() : KWQMapIteratorImpl()
    {
    }

    QMapConstIterator(const QMapConstIterator<K,V> &citer) : KWQMapIteratorImpl(citer)
    {
    }

    QMapConstIterator(const QMapIterator<K,V> &iter) : KWQMapIteratorImpl(iter)
    {
    }

    ~QMapConstIterator()
    {
    }

    const K& key() const
    {
	return ((QMapNode<K,V> *)node)->key;
    }

    const V& data() const
    {
	return ((QMapNode<K,V> *)node)->value;
    }

    QMapConstIterator<K,V> &operator=(const QMapConstIterator<K,V> &citer)
    {
	node = citer.node;
	return *this;
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
}; // class QMapConstIterator ==================================================



// class QMap ==================================================================

template <class K, class V> class QMap : public KWQMapImpl {
public:

    // typedefs ----------------------------------------------------------------

    typedef QMapIterator<K,V> Iterator;
    typedef QMapConstIterator<K,V> ConstIterator;

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------


    QMap() : 
	KWQMapImpl(new QMapNode<K,V>(K(),V()), deleteNode)
    {
    }

    QMap(const QMap<K,V>& m) :
	KWQMapImpl(m)
    {
    }

    virtual ~QMap() 
    { 
    }
    
    // member functions --------------------------------------------------------

    void clear() 
    {
	clearInternal();
    }

    uint count() const
    {
	return countInternal();
    }

    Iterator begin()
    {
	return Iterator((QMapNode<K,V> *)beginInternal());
    }

    Iterator end()
    {
	return Iterator((QMapNode<K,V> *)endInternal());
    }

    ConstIterator begin() const
    {
	return ConstIterator((QMapNode<K,V> *)beginInternal());
    }

    ConstIterator end() const
    {
	return ConstIterator((QMapNode<K,V> *)endInternal());
    }

    Iterator insert(const K& key, const V& value)
    {
	QMapNode<K,V> tmp(key,value);

	return Iterator((QMapNode<K,V> *)insertInternal(&tmp, true));
    }

    void remove(const K& key)
    {
	QMapNode<K,V> tmp(key, V());
	removeInternal(&tmp);
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

    // operators ---------------------------------------------------------------

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

}; // class QMap ===============================================================


#ifdef _KWQ_IOSTREAM_
template<class K, class V>
inline ostream &operator<<(ostream &stream, const QMap<K,V> &m) 
{
    uint count = m.count();
    stream << "QMap: [size: " << count << "; items: ";

    QMapConstIterator<K,V> iter = m.begin();
    
    for (unsigned int i = 0; i < count; i++) {
	stream << "(" << iter.key() << "," << iter.data() << ")";
	if (i + 1 < count) {
	    stream << ", ";
	}
	++iter;
    }

    return stream << "]";
}
#endif

#endif // USING_BORROWED_QMAP

#endif
