/****************************************************************************
** $Id$
**
** Definition of QMap class
**
** Created : 990406
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the tools module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#ifndef QMAP_H
#define QMAP_H

#ifndef QT_H
#include "_qshared.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif

// KWQ hacks ---------------------------------------------------------------

#include <iostream>

// -------------------------------------------------------------------------

#endif // QT_H


struct QMapNodeBase
{
    enum Color { Red, Black };

    QMapNodeBase* left;
    QMapNodeBase* right;
    QMapNodeBase* parent;

    Color color;

    QMapNodeBase* minimum() {
	QMapNodeBase* x = this;
	while ( x->left )
	    x = x->left;
	return x;
    }

    QMapNodeBase* maximum() {
	QMapNodeBase* x = this;
	while ( x->right )
	    x = x->right;
	return x;
    }
};


template <class K, class T>
struct QMapNode : public QMapNodeBase
{
    QMapNode( const K& _key, const T& _data ) { data = _data; key = _key; }
    QMapNode( const K& _key )	   { key = _key; }
    QMapNode( const QMapNode<K,T>& _n ) { key = _n.key; data = _n.data; }
    QMapNode() { }
    T data;
    K key;
};


template<class K, class T>
class Q_EXPORT QMapIterator
{
 public:
    /**
     * Typedefs
     */
    typedef QMapNode< K, T >* NodePtr;

    /**
     * Variables
     */
    QMapNode<K,T>* node;

    /**
     * Functions
     */
    QMapIterator() : node( 0 ) {}
    QMapIterator( QMapNode<K,T>* p ) : node( p ) {}
    QMapIterator( const QMapIterator<K,T>& it ) : node( it.node ) {}

    bool operator==( const QMapIterator<K,T>& it ) const { return node == it.node; }
    bool operator!=( const QMapIterator<K,T>& it ) const { return node != it.node; }
    T& operator*() { return node->data; }
    const T& operator*() const { return node->data; }

    // Cannot have this - some compilers are too stupid
    //T* operator->() const { return &(node->data); }

    const K& key() const { return node->key; }
    T& data() { return node->data; }
    const T& data() const { return node->data; }

private:
    int inc() {
	QMapNodeBase* tmp = node;
	if ( tmp->right ) {
	    tmp = tmp->right;
	    while ( tmp->left )
		tmp = tmp->left;
	} else {
	    QMapNodeBase* y = tmp->parent;
	    while (tmp == y->right) {
		tmp = y;
		y = y->parent;
	    }
	    if (tmp->right != y)
		tmp = y;
	}
	node = (NodePtr)tmp;
	return 0;
    }

    int dec() {
	QMapNodeBase* tmp = node;
	if (tmp->color == QMapNodeBase::Red &&
	    tmp->parent->parent == tmp ) {
	    tmp = tmp->right;
	} else if (tmp->left != 0) {
	    QMapNodeBase* y = tmp->left;
	    while ( y->right )
		y = y->right;
	    tmp = y;
	} else {
	    QMapNodeBase* y = tmp->parent;
	    while (tmp == y->left) {
		tmp = y;
		y = y->parent;
	    }
	    tmp = y;
	}
	node = (NodePtr)tmp;
	return 0;
    }

public:
    QMapIterator<K,T>& operator++() {
	inc();
	return *this;
    }

    QMapIterator<K,T> operator++(int) {
	QMapIterator<K,T> tmp = *this;
	inc();
	return tmp;
    }

    QMapIterator<K,T>& operator--() {
	dec();
	return *this;
    }

    QMapIterator<K,T> operator--(int) {
	QMapIterator<K,T> tmp = *this;
	dec();
	return tmp;
    }
};

template<class K, class T>
class Q_EXPORT QMapConstIterator
{
 public:
    /**
     * Typedefs
     */
    typedef QMapNode< K, T >* NodePtr;

    /**
     * Variables
     */
    QMapNode<K,T>* node;

    /**
     * Functions
     */
    QMapConstIterator() : node( 0 ) {}
    QMapConstIterator( QMapNode<K,T>* p ) : node( p ) {}
    QMapConstIterator( const QMapConstIterator<K,T>& it ) : node( it.node ) {}
    QMapConstIterator( const QMapIterator<K,T>& it ) : node( it.node ) {}

    bool operator==( const QMapConstIterator<K,T>& it ) const { return node == it.node; }
    bool operator!=( const QMapConstIterator<K,T>& it ) const { return node != it.node; }
    const T& operator*()  const { return node->data; }

    // Cannot have this - some compilers are too stupid
    //const T* operator->() const { return &(node->data); }

    const K& key() const { return node->key; }
    const T& data() const { return node->data; }

private:
    int inc() {
        QMapNodeBase* tmp = node;
	if ( tmp->right ) {
	    tmp = tmp->right;
	    while ( tmp->left )
		tmp = tmp->left;
	} else {
	    QMapNodeBase* y = tmp->parent;
	    while (tmp == y->right) {
		tmp = y;
		y = y->parent;
	    }
	    if (tmp->right != y)
		tmp = y;
	}
	node = (NodePtr)tmp;
	return 0;
    }

    int dec() {
	QMapNodeBase* tmp = node;
	if (tmp->color == QMapNodeBase::Red &&
	    tmp->parent->parent == tmp ) {
	    tmp = tmp->right;
	} else if (tmp->left != 0) {
	    QMapNodeBase* y = tmp->left;
	    while ( y->right )
		y = y->right;
	    tmp = y;
	} else {
	    QMapNodeBase* y = tmp->parent;
	    while (tmp == y->left) {
		tmp = y;
		y = y->parent;
	    }
	    tmp = y;
	}
	node = (NodePtr)tmp;
	return 0;
    }

public:
    QMapConstIterator<K,T>& operator++() {
	inc();
	return *this;
    }

    QMapConstIterator<K,T> operator++(int) {
	QMapConstIterator<K,T> tmp = *this;
	inc();
	return tmp;
    }

    QMapConstIterator<K,T>& operator--() {
	dec();
	return *this;
    }

    QMapConstIterator<K,T> operator--(int) {
	QMapConstIterator<K,T> tmp = *this;
	dec();
	return tmp;
    }
};


class Q_EXPORT QMapPrivateBase : public QShared
{
public:
    QMapPrivateBase() {
	node_count = 0;
    }
    QMapPrivateBase( const QMapPrivateBase* _map) {
	node_count = _map->node_count;
    }

    /**
     * Implementations of basic tree algorithms
     */
    void rotateLeft( QMapNodeBase* x, QMapNodeBase*& root);
    void rotateRight( QMapNodeBase* x, QMapNodeBase*& root );
    void rebalance( QMapNodeBase* x, QMapNodeBase*& root );
    QMapNodeBase* removeAndRebalance( QMapNodeBase* z, QMapNodeBase*& root,
				      QMapNodeBase*& leftmost,
				      QMapNodeBase*& rightmost );

    /**
     * Variables
     */
    int node_count;
};


template <class Key, class T>
class QMapPrivate : public QMapPrivateBase
{
public:
    /**
     * Typedefs
     */
    typedef QMapIterator< Key, T > Iterator;
    typedef QMapConstIterator< Key, T > ConstIterator;
    typedef QMapNode< Key, T > Node;
    typedef QMapNode< Key, T >* NodePtr;

    /**
     * Functions
     */
    QMapPrivate() {
	header = new Node;
	header->color = QMapNodeBase::Red; // Mark the header
	header->parent = 0;
	header->left = header->right = header;
    }
    QMapPrivate( const QMapPrivate< Key, T >* _map ) : QMapPrivateBase( _map ) {
	header = new Node;
	header->color = QMapNodeBase::Red; // Mark the header
	if ( _map->header->parent == 0 ) {
	    header->parent = 0;
	    header->left = header->right = header;
	} else {
	    header->parent = copy( (NodePtr)(_map->header->parent) );
	    header->parent->parent = header;
	    header->left = header->parent->minimum();
	    header->right = header->parent->maximum();
	}
    }
    ~QMapPrivate() { clear(); delete header; }

    NodePtr copy( NodePtr p ) {
	if ( !p )
	    return 0;
	NodePtr n = new Node( *p );
	n->color = p->color;
	if ( p->left ) {
	    n->left = copy( (NodePtr)(p->left) );
	    n->left->parent = n;
	} else {
	    n->left = 0;
	}
	if ( p->right ) {
	    n->right = copy( (NodePtr)(p->right) );
	    n->right->parent = n;
	} else {
	    n->right = 0;
	}
	return n;
    }

    void clear() {
	clear( (NodePtr)(header->parent) );
	header->color = QMapNodeBase::Red;
	header->parent = 0;
	header->left = header->right = header;
	node_count = 0;
    }

    void clear( NodePtr p ) {
	while ( p != 0 ) {
	    clear( (NodePtr)p->right );
	    NodePtr y = (NodePtr)p->left;
	    delete p;
	    p = y;
	}
    }

    Iterator begin()	{ return Iterator( (NodePtr)(header->left ) ); }
    Iterator end()	{ return Iterator( header ); }
    ConstIterator begin() const { return ConstIterator( (NodePtr)(header->left ) ); }
    ConstIterator end() const { return ConstIterator( header ); }

    ConstIterator find(const Key& k) const {
	QMapNodeBase* y = header;        // Last node
	QMapNodeBase* x = header->parent; // Root node.

	while ( x != 0 ) {
	    // If as k <= key(x) go left
	    if ( !( key(x) < k ) ) {
		y = x;
		x = x->left;
	    } else {
		x = x->right;
	    }
	}

	// Was k bigger/smaller then the biggest/smallest
	// element of the tree ? Return end()
	if ( y == header || k < key(y) )
	    return ConstIterator( header );
	return ConstIterator( (NodePtr)y );
    }

    void remove( Iterator it ) {
	NodePtr del = (NodePtr) removeAndRebalance( it.node, header->parent, header->left, header->right );
	delete del;
	--node_count;
    }

#ifdef QT_QMAP_DEBUG
    void inorder( QMapNodeBase* x = 0, int level = 0 ){
	if ( !x )
	    x = header->parent;
	if ( x->left )
	    inorder( x->left, level + 1 );
    //cout << level << " Key=" << key(x) << " Value=" << ((NodePtr)x)->data << endl;
	if ( x->right )
	    inorder( x->right, level + 1 );
    }
#endif

    Iterator insertMulti(const Key& v){
	QMapNodeBase* y = header;
	QMapNodeBase* x = header->parent;
	while (x != 0){
	    y = x;
	    x = ( v < key(x) ) ? x->left : x->right;
	}
	return insert(x, y, v);
    }

    Iterator insertSingle( const Key& k ) {
	// Search correct position in the tree
	QMapNodeBase* y = header;
	QMapNodeBase* x = header->parent;
	bool result = TRUE;
	while ( x != 0 ) {
	    result = ( k < key(x) );
	    y = x;
	    x = result ? x->left : x->right;
	}
	// Get iterator on the last not empty one
	Iterator j( (NodePtr)y );
	if ( result ) {
	    // Smaller then the leftmost one ?
	    if ( j == begin() ) {
		return insert(x, y, k );
	    } else {
		// Perhaps daddy is the right one ?
		--j;
	    }
	}
	// Really bigger ?
	if ( (j.node->key) < k )
	    return insert(x, y, k );
	// We are going to replace a node
	return j;
    }

    Iterator insert( QMapNodeBase* x, QMapNodeBase* y, const Key& k ) {
	NodePtr z = new Node( k );
	if (y == header || x != 0 || k < key(y) ) {
	    y->left = z;                // also makes leftmost = z when y == header
	    if ( y == header ) {
		header->parent = z;
		header->right = z;
	    } else if ( y == header->left )
		header->left = z;           // maintain leftmost pointing to min node
	} else {
	    y->right = z;
	    if ( y == header->right )
		header->right = z;          // maintain rightmost pointing to max node
	}
	z->parent = y;
	z->left = 0;
	z->right = 0;
	rebalance( z, header->parent );
	++node_count;
	return Iterator(z);
    }

protected:
    /**
     * Helpers
     */
    const Key& key( QMapNodeBase* b ) const { return ((NodePtr)b)->key; }

    /**
     * Variables
     */
    NodePtr header;
};


template<class Key, class T>
class Q_EXPORT QMap
{
public:
    /**
     * Typedefs
     */
    typedef QMapIterator< Key, T > Iterator;
    typedef QMapConstIterator< Key, T > ConstIterator;
    typedef T ValueType;
    typedef QMapPrivate< Key, T > Priv;

    /**
     * API
     */
    QMap() { sh = new QMapPrivate< Key, T >; }
    QMap( const QMap<Key,T>& m ) { sh = m.sh; sh->ref(); }
    ~QMap() { if ( sh->deref() ) delete sh; }

    QMap<Key,T>& operator= ( const QMap<Key,T>& m )
      { m.sh->ref(); if ( sh->deref() ) delete sh; sh = m.sh; return *this; }

    Iterator begin() { detach(); return sh->begin(); }
    Iterator end() { detach(); return sh->end(); }
    ConstIterator begin() const { return ((const Priv*)sh)->begin(); }
    ConstIterator end() const { return ((const Priv*)sh)->end(); }

    Iterator find ( const Key& k )
	{ detach(); return Iterator( sh->find( k ).node ); }
    ConstIterator find ( const Key& k ) const
	{ return sh->find( k ); }
    T& operator[] ( const Key& k ) {
	detach(); QMapNode<Key,T>* p = sh->find( k ).node;
	if ( p != sh->end().node ) return p->data;
	return insert( k, T() ).data(); }
    const T& operator[] ( const Key& k ) const
	{ return sh->find( k ).data(); }
    bool contains ( const Key& k ) const
	{ return find( k ) != end(); }
	//{ return sh->find( k ) != ((const Priv*)sh)->end(); }

    uint count() const { return sh->node_count; }

    bool isEmpty() const { return sh->node_count == 0; }

    Iterator insert( const Key& key, const T& value ) {
        detach();
        Iterator it = sh->insertSingle( key );
        it.data() = value;
        return it;
    }

    void remove( Iterator it ) { detach(); sh->remove( it ); }
    void remove( const Key& k ) {
        detach();
        Iterator it( sh->find( k ).node );
        if ( it != end() )
            sh->remove( it );
    }

    Iterator replace( const Key& k, const T& v ) {
	remove( k );
	return insert( k, v );
    }

    void clear() { if ( sh->count == 1 ) sh->clear(); else { sh->deref(); sh = new QMapPrivate<Key,T>; } }

#if defined(Q_FULL_TEMPLATE_INSTANTIATION)
    bool operator==( const QMap<Key,T>& ) const { return FALSE; }
#endif

protected:
    /**
     * Helpers
     */
    void detach() { if ( sh->count > 1 ) { sh->deref(); sh = new QMapPrivate<Key,T>( sh ); } }

    Priv* sh;
};


#ifdef _KWQ_IOSTREAM_
template<class Key, class T>
inline ostream &operator<<(ostream &o, const QMap<Key,T>&m) 
{
    o << 
        "QMap: [size: " <<
        (Q_UINT32)m.count() <<
        "; items: ";
        QMapConstIterator<Key,T> it = m.begin();
        while (it != m.end()) {
            o << "(" << it.key() << "," << it.data() << ")";
            if (++it != m.end()) {
                o << ", ";
            }
        }
        o << "]";
    return o;
}
#endif


#ifndef QT_NO_DATASTREAM
template<class Key, class T>
inline QDataStream& operator>>( QDataStream& s, QMap<Key,T>& m ) {
    m.clear();
    Q_UINT32 c;
    s >> c;
    for( Q_UINT32 i = 0; i < c; ++i ) {
	Key k; T t;
	s >> k >> t;
	m.insert( k, t );
    }
    return s;
}


template<class Key, class T>
inline QDataStream& operator<<( QDataStream& s, const QMap<Key,T>& m ) {
    s << (Q_UINT32)m.count();
    QMapConstIterator<Key,T> it = m.begin();
    for( ; it != m.end(); ++it )
	s << it.key() << it.data();
    return s;
}
#endif

#endif // QMAP_H
