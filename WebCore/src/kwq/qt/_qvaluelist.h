/****************************************************************************
** $Id$
**
** Definition of QValueList class
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

#ifndef QVALUELIST_H
#define QVALUELIST_H

// KWQ hacks ---------------------------------------------------------------

#ifndef _KWQ_COMPLETE_
#define _KWQ_COMPLETE_
#endif

#include <KWQDef.h>
#include <iostream>

// -------------------------------------------------------------------------

#ifndef QT_H
#include "_qshared.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif

#endif // QT_H

#if defined(_CC_MSVC_)
#pragma warning(disable:4284) // "return type for operator -> is not a UDT"
#endif

template <class T>
class Q_EXPORT QValueListNode
{
public:
    QValueListNode( const T& t ) : data( t ) { }
    QValueListNode() { }
#if defined(Q_TEMPLATEDLL)
    // Workaround MS bug in memory de/allocation in DLL vs. EXE 
    virtual ~QValueListNode() { }
#endif

    QValueListNode<T>* next;
    QValueListNode<T>* prev;
    T data;
};

template<class T>
class Q_EXPORT QValueListIterator
{
 public:
    /**
     * Typedefs
     */
    typedef QValueListNode<T>* NodePtr;

    /**
     * Variables
     */
    NodePtr node;

    /**
     * Functions
     */
    QValueListIterator() : node( 0 ) {}
    QValueListIterator( NodePtr p ) : node( p ) {}
    QValueListIterator( const QValueListIterator<T>& it ) : node( it.node ) {}

    bool operator==( const QValueListIterator<T>& it ) const { return node == it.node; }
    bool operator!=( const QValueListIterator<T>& it ) const { return node != it.node; }
    const T& operator*() const { return node->data; }
    T& operator*() { return node->data; }

    // Compilers are too dumb to understand this for QValueList<int>
    //T* operator->() const { return &(node->data); }

    QValueListIterator<T>& operator++() {
	node = node->next;
	return *this;
    }

    QValueListIterator<T> operator++(int) {
	QValueListIterator<T> tmp = *this;
	node = node->next;
	return tmp;
    }

    QValueListIterator<T>& operator--() {
	node = node->prev;
	return *this;
    }

    QValueListIterator<T> operator--(int) {
	QValueListIterator<T> tmp = *this;
	node = node->prev;
	return tmp;
    }
};

template<class T>
class Q_EXPORT QValueListConstIterator
{
 public:
    /**
     * Typedefs
     */
    typedef QValueListNode<T>* NodePtr;

    /**
     * Variables
     */
    NodePtr node;

    /**
     * Functions
     */
    QValueListConstIterator() : node( 0 ) {}
    QValueListConstIterator( NodePtr p ) : node( p ) {}
    QValueListConstIterator( const QValueListConstIterator<T>& it ) : node( it.node ) {}
    QValueListConstIterator( const QValueListIterator<T>& it ) : node( it.node ) {}

    bool operator==( const QValueListConstIterator<T>& it ) const { return node == it.node; }
    bool operator!=( const QValueListConstIterator<T>& it ) const { return node != it.node; }
    const T& operator*() const { return node->data; }

    // Compilers are too dumb to understand this for QValueList<int>
    //const T* operator->() const { return &(node->data); }

    QValueListConstIterator<T>& operator++() {
	node = node->next;
	return *this;
    }

    QValueListConstIterator<T> operator++(int) {
	QValueListConstIterator<T> tmp = *this;
	node = node->next;
	return tmp;
    }

    QValueListConstIterator<T>& operator--() {
	node = node->prev;
	return *this;
    }

    QValueListConstIterator<T> operator--(int) {
	QValueListConstIterator<T> tmp = *this;
	node = node->prev;
	return tmp;
    }
};

template <class T>
class Q_EXPORT QValueListPrivate : public QShared
{
public:
    /**
     * Typedefs
     */
    typedef QValueListIterator<T> Iterator;
    typedef QValueListConstIterator<T> ConstIterator;
    typedef QValueListNode<T> Node;
    typedef QValueListNode<T>* NodePtr;

    /**
     * Functions
     */
    QValueListPrivate() { node = new Node; node->next = node->prev = node; nodes = 0; }
    QValueListPrivate( const QValueListPrivate<T>& _p ) : QShared() {
	node = new Node; node->next = node->prev = node; nodes = 0;
	Iterator b( _p.node->next );
	Iterator e( _p.node );
	Iterator i( node );
	while( b != e )
	    insert( i, *b++ );
    }

    void derefAndDelete() // ### hack to get around hp-cc brain damage
    {
	if ( deref() )
	    delete this;
    }

#if defined(Q_TEMPLATEDLL)
    // Workaround MS bug in memory de/allocation in DLL vs. EXE 
    virtual
#endif
    ~QValueListPrivate() {
	NodePtr p = node->next;
	while( p != node ) {
	    NodePtr x = p->next;
	    delete p;
	    p = x;
	}
	delete node;
    }

    Iterator insert( Iterator it, const T& x ) {
	NodePtr p = new Node( x );
	p->next = it.node;
	p->prev = it.node->prev;
	it.node->prev->next = p;
	it.node->prev = p;
	nodes++;
	return p;
    }

    Iterator remove( Iterator it ) {
	ASSERT ( it.node != node );
	NodePtr next = it.node->next;
	NodePtr prev = it.node->prev;
	prev->next = next;
	next->prev = prev;
	delete it.node;
	nodes--;
	return Iterator( next );
    }

    NodePtr find( NodePtr start, const T& x ) const {
	ConstIterator first( start );
	ConstIterator last( node );
	while( first != last) {
	    if ( *first == x )
		return first.node;
	    ++first;
	}
	return last.node;
    }

    int findIndex( NodePtr start, const T& x ) const {
	ConstIterator first( start );
	ConstIterator last( node );
	int pos = 0;
	while( first != last) {
	    if ( *first == x )
		return pos;
	    ++first;
	    ++pos;
	}
	return -1;
    }

    uint contains( const T& x ) const {
	uint result = 0;
	Iterator first = Iterator( node->next );
	Iterator last = Iterator( node );
	while( first != last) {
	    if ( *first == x )
		++result;
	    ++first;
	}
	return result;
    }

    void remove( const T& x ) {
	Iterator first = Iterator( node->next );
	Iterator last = Iterator( node );
	while( first != last) {
	    if ( *first == x )
		first = remove( first );
	    else
		++first;
	}
    }

    NodePtr at( uint i ) const {
	ASSERT( i <= nodes );
	NodePtr p = node->next;
	for( uint x = 0; x < i; ++x )
	    p = p->next;
	return p;
    }

    void clear() {
	nodes = 0;
	NodePtr p = node->next;
	while( p != node ) {
	    NodePtr next = p->next;
	    delete p;
	    p = next;
	}
	node->next = node->prev = node;
    }

    NodePtr node;
    uint nodes;
};

template <class T>
class Q_EXPORT QValueList
{
public:
    /**
     * Typedefs
     */
    typedef QValueListIterator<T> Iterator;
    typedef QValueListConstIterator<T> ConstIterator;
    typedef T ValueType;

    /**
     * API
     */
    QValueList() { sh = new QValueListPrivate<T>; }
    QValueList( const QValueList<T>& l ) { sh = l.sh; sh->ref(); }
    ~QValueList() { sh->derefAndDelete(); }

    QValueList<T>& operator= ( const QValueList<T>& l )
    {
	l.sh->ref();
	sh->derefAndDelete();
	sh = l.sh;
	return *this;
    }

    QValueList<T> operator+ ( const QValueList<T>& l ) const
    {
	QValueList<T> l2( *this );
	for( ConstIterator it = l.begin(); it != l.end(); ++it )
	    l2.append( *it );
	return l2;
    }

    QValueList<T>& operator+= ( const QValueList<T>& l )
    {
	for( ConstIterator it = l.begin(); it != l.end(); ++it )
	    append( *it );
	return *this;
    }

    bool operator== ( const QValueList<T>& l ) const
    {
	if ( count() != l.count() )
	    return FALSE;
	ConstIterator it2 = begin();
	ConstIterator it = l.begin();
	for( ; it != l.end(); ++it, ++it2 )
	    if ( !( *it == *it2 ) )
		return FALSE;
	return TRUE;
    }

    bool operator!= ( const QValueList<T>& l ) const { return !( *this == l ); }

    Iterator begin() { detach(); return Iterator( sh->node->next ); }
    ConstIterator begin() const { return ConstIterator( sh->node->next ); }
    Iterator end() { detach(); return Iterator( sh->node ); }
    ConstIterator end() const { return ConstIterator( sh->node ); }
    Iterator fromLast() { detach(); return Iterator( sh->node->prev ); }
    ConstIterator fromLast() const { return ConstIterator( sh->node->prev ); }

    bool isEmpty() const { return ( sh->nodes == 0 ); }

    Iterator insert( Iterator it, const T& x ) { detach(); return sh->insert( it, x ); }

    Iterator append( const T& x ) { detach(); return sh->insert( end(), x ); }
    Iterator prepend( const T& x ) { detach(); return sh->insert( begin(), x ); }

    Iterator remove( Iterator it ) { detach(); return sh->remove( it ); }
    void remove( const T& x ) { detach(); sh->remove( x ); }

    T& first() { detach(); return sh->node->next->data; }
    const T& first() const { return sh->node->next->data; }
    T& last() { detach(); return sh->node->prev->data; }
    const T& last() const { return sh->node->prev->data; }

    T& operator[] ( uint i ) { detach(); return sh->at(i)->data; }
    const T& operator[] ( uint i ) const { return sh->at(i)->data; }
    Iterator at( uint i ) { detach(); return Iterator( sh->at(i) ); }
    ConstIterator at( uint i ) const { return ConstIterator( sh->at(i) ); }
    Iterator find ( const T& x ) { detach(); return Iterator( sh->find( sh->node->next, x) ); }
    ConstIterator find ( const T& x ) const { return ConstIterator( sh->find( sh->node->next, x) ); }
    Iterator find ( Iterator it, const T& x ) { detach(); return Iterator( sh->find( it.node, x ) ); }
    ConstIterator find ( ConstIterator it, const T& x ) const { return ConstIterator( sh->find( it.node, x ) ); }
    int findIndex( const T& x ) const { return sh->findIndex( sh->node->next, x) ; }
    uint contains( const T& x ) const { return sh->contains( x ); }

    uint count() const { return sh->nodes; }

    void clear() { if ( sh->count == 1 ) sh->clear(); else { sh->deref(); sh = new QValueListPrivate<T>; } }


    QValueList<T>& operator+= ( const T& x )
    {
	append( x );
	return *this;
    }
    QValueList<T>& operator<< ( const T& x )
    {
	append( x );
	return *this;
    }


protected:
    /**
     * Helpers
     */
    void detach() { if ( sh->count > 1 ) { sh->deref(); sh = new QValueListPrivate<T>( *sh ); } }

    /**
     * Variables
     */
    QValueListPrivate<T>* sh;
};


#ifdef _KWQ_IOSTREAM_
template<class T>
inline ostream &operator<<(ostream &o, const QValueList<T>&p)
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
#endif


#ifndef QT_NO_DATASTREAM
template<class T>
inline QDataStream& operator>>( QDataStream& s, QValueList<T>& l )
{
    l.clear();
    Q_UINT32 c;
    s >> c;
    for( Q_UINT32 i = 0; i < c; ++i )
    {
	T t;
	s >> t;
	l.append( t );
    }
    return s;
}

template<class T>
inline QDataStream& operator<<( QDataStream& s, const QValueList<T>& l )
{
    s << (Q_UINT32)l.count();
    QValueListConstIterator<T> it = l.begin();
    for( ; it != l.end(); ++it )
	s << *it;
    return s;
}
#endif // QT_NO_DATASTREAM
#endif // QVALUELIST_H
