/****************************************************************************
** $Id$
**
** Definition of QDict template class
**
** Created : 920821
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

#ifndef QDICT_H
#define QDICT_H

// KWQ hacks ---------------------------------------------------------------

#ifdef USING_BORROWED_QDICT

#include <KWQDef.h>
#include <iostream>

// -------------------------------------------------------------------------

#ifndef QT_H
#include "_qgdict.h"
#endif // QT_H


template<class type> class Q_EXPORT QDict : public QGDict
{
public:
    QDict(int size=17, bool caseSensitive=TRUE)
	: QGDict(size,StringKey,caseSensitive,FALSE) {}
    QDict( const QDict<type> &d ) : QGDict(d) {}
   ~QDict()				{ clear(); }
    QDict<type> &operator=(const QDict<type> &d)
			{ return (QDict<type>&)QGDict::operator=(d); }
    uint  count()   const		{ return QGDict::count(); }
    uint  size()    const		{ return QGDict::size(); }
    bool  isEmpty() const		{ return QGDict::count() == 0; }

    void  insert( const QString &k, const type *d )
					{ QGDict::look_string(k,(Item)d,1); }
    void  replace( const QString &k, const type *d )
					{ QGDict::look_string(k,(Item)d,2); }
    bool  remove( const QString &k )	{ return QGDict::remove_string(k); }
    type *take( const QString &k )	{ return (type *)QGDict::take_string(k); }
    type *find( const QString &k ) const
		{ return (type *)((QGDict*)this)->QGDict::look_string(k,0,0); }
    type *operator[]( const QString &k ) const
		{ return (type *)((QGDict*)this)->QGDict::look_string(k,0,0); }

    void  clear()			{ QGDict::clear(); }
    void  resize( uint n )		{ QGDict::resize(n); }
    void  statistics() const		{ QGDict::statistics(); }

private:
    void  deleteItem( Item d );
};

#if defined(Q_DELETING_VOID_UNDEFINED)
template<> inline void QDict<void>::deleteItem( Item )
{
}
#endif

template<class type> inline void QDict<type>::deleteItem( QCollection::Item d )
{
    if ( del_item ) delete (type *)d;
}


template<class type> class Q_EXPORT QDictIterator : public QGDictIterator
{
public:
    QDictIterator(const QDict<type> &d) :QGDictIterator((QGDict &)d) {}
   ~QDictIterator()	      {}
    uint  count()   const     { return dict->count(); }
    bool  isEmpty() const     { return dict->count() == 0; }
    type *toFirst()	      { return (type *)QGDictIterator::toFirst(); }
    operator type *() const   { return (type *)QGDictIterator::get(); }
    type   *current() const   { return (type *)QGDictIterator::get(); }
    QString currentKey() const{ return QGDictIterator::getKeyString(); }
    type *operator()()	      { return (type *)QGDictIterator::operator()(); }
    type *operator++()	      { return (type *)QGDictIterator::operator++(); }
    type *operator+=(uint j)  { return (type *)QGDictIterator::operator+=(j);}
};

#if 0
#ifdef _KWQ_IOSTREAM_
template <class T>
ostream &operator<<(ostream &o, const QDict<T>&d)
{
    o <<
        "QDict: [size: " <<
        (Q_UINT32)d.count() <<
        "; items: ";
        QDictIterator<T> it = QDictIterator<T>(d);
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
#endif

#endif

#endif // QDICT_H
