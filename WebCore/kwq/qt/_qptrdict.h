/****************************************************************************
** $Id$
**
** Definition of QPtrDict template class
**
** Created : 970415
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

#ifndef QPTRDICT_H
#define QPTRDICT_H

// KWQ hacks ---------------------------------------------------------------

#ifndef _KWQ_COMPLETE_
#define _KWQ_COMPLETE_
#endif

#include <KWQDef.h>
#include <iostream>

// -------------------------------------------------------------------------

#ifndef QT_H
#include "_qgdict.h"
#endif // QT_H


template<class type> class Q_EXPORT QPtrDict : public QGDict
{
public:
    QPtrDict(int size=17) : QGDict(size,PtrKey,0,0) {}
    QPtrDict( const QPtrDict<type> &d ) : QGDict(d) {}
   ~QPtrDict()				{ clear(); }
    QPtrDict<type> &operator=(const QPtrDict<type> &d)
			{ return (QPtrDict<type>&)QGDict::operator=(d); }
    uint  count()   const		{ return QGDict::count(); }
    uint  size()    const		{ return QGDict::size(); }
    bool  isEmpty() const		{ return QGDict::count() == 0; }
    void  insert( void *k, const type *d )
					{ QGDict::look_ptr(k,(Item)d,1); }
    void  replace( void *k, const type *d )
					{ QGDict::look_ptr(k,(Item)d,2); }
    bool  remove( void *k )		{ return QGDict::remove_ptr(k); }
    type *take( void *k )		{ return (type*)QGDict::take_ptr(k); }
    type *find( void *k ) const
		{ return (type *)((QGDict*)this)->QGDict::look_ptr(k,0,0); }
    type *operator[]( void *k ) const
		{ return (type *)((QGDict*)this)->QGDict::look_ptr(k,0,0); }
    void  clear()			{ QGDict::clear(); }
    void  resize( uint n )		{ QGDict::resize(n); }
    void  statistics() const		{ QGDict::statistics(); }
private:
    void  deleteItem( Item d );
};

#if defined(Q_DELETING_VOID_UNDEFINED)
template<> inline void QPtrDict<void>::deleteItem( QCollection::Item )
{
}
#endif

template<class type> inline void QPtrDict<type>::deleteItem( QCollection::Item d )
{
    if ( del_item ) delete (type *)d;
}


template<class type> class Q_EXPORT QPtrDictIterator : public QGDictIterator
{
public:
    QPtrDictIterator(const QPtrDict<type> &d) :QGDictIterator((QGDict &)d) {}
   ~QPtrDictIterator()	      {}
    uint  count()   const     { return dict->count(); }
    bool  isEmpty() const     { return dict->count() == 0; }
    type *toFirst()	      { return (type *)QGDictIterator::toFirst(); }
    operator type *()  const  { return (type *)QGDictIterator::get(); }
    type *current()    const  { return (type *)QGDictIterator::get(); }
    void *currentKey() const  { return QGDictIterator::getKeyPtr(); }
    type *operator()()	      { return (type *)QGDictIterator::operator()(); }
    type *operator++()	      { return (type *)QGDictIterator::operator++(); }
    type *operator+=(uint j)  { return (type *)QGDictIterator::operator+=(j);}
};

#ifdef _KWQ_IOSTREAM_
template <class T>
ostream &operator<<(ostream &o, const QPtrDict<T>&d)
{
    o <<
        "QPtrDict: [size: " <<
        (Q_UINT32)d.count() <<
        "; items: ";
        QPtrDictIterator<T> it = QPtrDictIterator<T>(d);
        int count = it.count();
        for (int i = 0; i < count; i++) {
            o << "(" << it.currentKey() << "," << *(it.current()) << ")";
            if (i < count - 1) {
                o << ", ";
            }
            ++it;
        }
        o << "]";
    return o;
}
#endif


#endif // QPTRDICT_H
