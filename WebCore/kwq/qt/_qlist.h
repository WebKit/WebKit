/****************************************************************************
** $Id$
**
** Definition of QList template/macro class
**
** Created : 920701
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

#ifndef QLIST_H
#define QLIST_H

// KWQ hacks ---------------------------------------------------------------

#ifndef _KWQ_COMPLETE_
#define _KWQ_COMPLETE_
#endif

#include <KWQDef.h>
#include <iostream>

// -------------------------------------------------------------------------

#ifndef QT_H
#include "_qglist.h"
#endif // QT_H


template<class type> class Q_EXPORT QList : public QGList
{
public:
    QList()				{}
    QList( const QList<type> &l ) : QGList(l) {}
   ~QList()				{ clear(); }
    QList<type> &operator=(const QList<type> &l)
			{ return (QList<type>&)QGList::operator=(l); }
    bool operator==( const QList<type> &list ) const
    { return QGList::operator==( list ); }
    uint  count()   const		{ return QGList::count(); }
    bool  isEmpty() const		{ return QGList::count() == 0; }
    bool  insert( uint i, const type *d){ return QGList::insertAt(i,(QCollection::Item)d); }
    void  inSort( const type *d )	{ QGList::inSort((QCollection::Item)d); }
    void  prepend( const type *d )	{ QGList::insertAt(0,(QCollection::Item)d); }
    void  append( const type *d )	{ QGList::append((QCollection::Item)d); }
    bool  remove( uint i )		{ return QGList::removeAt(i); }
    bool  remove()			{ return QGList::remove((QCollection::Item)0); }
    bool  remove( const type *d )	{ return QGList::remove((QCollection::Item)d); }
    bool  removeRef( const type *d )	{ return QGList::removeRef((QCollection::Item)d); }
    void  removeNode( QLNode *n )	{ QGList::removeNode(n); }
    bool  removeFirst()			{ return QGList::removeFirst(); }
    bool  removeLast()			{ return QGList::removeLast(); }
    type *take( uint i )		{ return (type *)QGList::takeAt(i); }
    type *take()			{ return (type *)QGList::take(); }
    type *takeNode( QLNode *n )		{ return (type *)QGList::takeNode(n); }
    void  clear()			{ QGList::clear(); }
    void  sort()			{ QGList::sort(); }
    int	  find( const type *d )		{ return QGList::find((QCollection::Item)d); }
    int	  findNext( const type *d )	{ return QGList::find((QCollection::Item)d,FALSE); }
    int	  findRef( const type *d )	{ return QGList::findRef((QCollection::Item)d); }
    int	  findNextRef( const type *d ){ return QGList::findRef((QCollection::Item)d,FALSE);}
    uint  contains( const type *d ) const { return QGList::contains((QCollection::Item)d); }
    uint  containsRef( const type *d ) const
					{ return QGList::containsRef((QCollection::Item)d); }
    type *at( uint i )			{ return (type *)QGList::at(i); }
    int	  at() const			{ return QGList::at(); }
    type *current()  const		{ return (type *)QGList::get(); }
    QLNode *currentNode()  const	{ return QGList::currentNode(); }
    type *getFirst() const		{ return (type *)QGList::cfirst(); }
    type *getLast()  const		{ return (type *)QGList::clast(); }
    type *first()			{ return (type *)QGList::first(); }
    type *last()			{ return (type *)QGList::last(); }
    type *next()			{ return (type *)QGList::next(); }
    type *prev()			{ return (type *)QGList::prev(); }
    void  toVector( QGVector *vec )const{ QGList::toVector(vec); }
private:
    void  deleteItem( QCollection::Item d );
};

#if defined(Q_DELETING_VOID_UNDEFINED)
template<> inline void QList<void>::deleteItem( QCollection::Item )
{
}
#endif

template<class type> inline void QList<type>::deleteItem( QCollection::Item d )
{
    if ( del_item ) delete (type *)d;
}


template<class type> class Q_EXPORT QListIterator : public QGListIterator
{
public:
    QListIterator(const QList<type> &l) :QGListIterator((QGList &)l) {}
   ~QListIterator()	      {}
    uint  count()   const     { return list->count(); }
    bool  isEmpty() const     { return list->count() == 0; }
    bool  atFirst() const     { return QGListIterator::atFirst(); }
    bool  atLast()  const     { return QGListIterator::atLast(); }
    type *toFirst()	      { return (type *)QGListIterator::toFirst(); }
    type *toLast()	      { return (type *)QGListIterator::toLast(); }
    operator type *() const   { return (type *)QGListIterator::get(); }
    type *operator*()         { return (type *)QGListIterator::get(); }

    // No good, since QList<char> (ie. QStrList fails...
    //
    // MSVC++ gives warning
    // Sunpro C++ 4.1 gives error
    //    type *operator->()        { return (type *)QGListIterator::get(); }

    type *current()   const   { return (type *)QGListIterator::get(); }
    type *operator()()	      { return (type *)QGListIterator::operator()();}
    type *operator++()	      { return (type *)QGListIterator::operator++(); }
    type *operator+=(uint j)  { return (type *)QGListIterator::operator+=(j);}
    type *operator--()	      { return (type *)QGListIterator::operator--(); }
    type *operator-=(uint j)  { return (type *)QGListIterator::operator-=(j);}
    QListIterator<type>& operator=(const QListIterator<type>&it)
			      { QGListIterator::operator=(it); return *this; }
};

#ifdef _KWQ_IOSTREAM_
template<class T>
inline ostream &operator<<(ostream &o, const QList<T> &p)
{
    QListIterator<T> it = QListIterator<T>(p);
    int count = it.count();

    o << "QList: [size: " << 
    count <<
    "; items: ";

    if (count == 0) {
        // no-op
    }
    else if (count == 1) {
        o << *(it.current());
    }
    else {
        o << *(it.current());
        while (!it.atLast()) {
            ++it;
            o << ", " << *(it.current());
        }
    }
    o << "]";

    return o;
}
#endif


#endif // QLIST_H
