/****************************************************************************
** $Id$
**
** Definition of QArray template/macro class
**
** Created : 930906
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

#ifndef QARRAY_H
#define QARRAY_H

#ifndef QT_H
#include "_qgarray.h"
#endif // QT_H

template<class type> class QArray : public QGArray
{
public:
    typedef type* Iterator;
    typedef const type* ConstIterator;
    typedef type ValueType;

protected:
    QArray( int, int ) : QGArray( 0, 0 ) {}

public:
    QArray() {}
    QArray( int size ) : QGArray(size*sizeof(type)) {}
    QArray( const QArray<type> &a ) : QGArray(a) {}
   ~QArray() {}
    QArray<type> &operator=(const QArray<type> &a)
				{ return (QArray<type>&)QGArray::assign(a); }
    type *data()    const	{ return (type *)QGArray::data(); }
    uint  nrefs()   const	{ return QGArray::nrefs(); }
    uint  size()    const	{ return QGArray::size()/sizeof(type); }
    uint  count()   const 	{ return size(); }
    bool  isEmpty() const	{ return QGArray::size() == 0; }
    bool  isNull()  const	{ return QGArray::data() == 0; }
    bool  resize( uint size )	{ return QGArray::resize(size*sizeof(type)); }
    bool  truncate( uint pos )	{ return QGArray::resize(pos*sizeof(type)); }
    bool  fill( const type &d, int size = -1 )
	{ return QGArray::fill((char*)&d,size,sizeof(type) ); }
    void  detach()		{ QGArray::detach(); }
    QArray<type>   copy() const
	{ QArray<type> tmp; return tmp.duplicate(*this); }
    QArray<type>& assign( const QArray<type>& a )
	{ return (QArray<type>&)QGArray::assign(a); }
    QArray<type>& assign( const type *a, uint n )
	{ return (QArray<type>&)QGArray::assign((char*)a,n*sizeof(type)); }
    QArray<type>& duplicate( const QArray<type>& a )
	{ return (QArray<type>&)QGArray::duplicate(a); }
    QArray<type>& duplicate( const type *a, uint n )
	{ return (QArray<type>&)QGArray::duplicate((char*)a,n*sizeof(type)); }
    QArray<type>& setRawData( const type *a, uint n )
	{ return (QArray<type>&)QGArray::setRawData((char*)a,
						     n*sizeof(type)); }
    void resetRawData( const type *a, uint n )
	{ QGArray::resetRawData((char*)a,n*sizeof(type)); }
    int	 find( const type &d, uint i=0 ) const
	{ return QGArray::find((char*)&d,i,sizeof(type)); }
    int	 contains( const type &d ) const
	{ return QGArray::contains((char*)&d,sizeof(type)); }
    void sort() { QGArray::sort(sizeof(type)); }
    int  bsearch( const type &d ) const
	{ return QGArray::bsearch((const char*)&d,sizeof(type)); }
    type& operator[]( int i ) const
	{ return (type &)(*(type *)QGArray::at(i*sizeof(type))); }
    type& at( uint i ) const
	{ return (type &)(*(type *)QGArray::at(i*sizeof(type))); }
	 operator const type*() const { return (const type *)QGArray::data(); }
    bool operator==( const QArray<type> &a ) const { return isEqual(a); }
    bool operator!=( const QArray<type> &a ) const { return !isEqual(a); }
    Iterator begin() { return data(); }
    Iterator end() { return data() + size(); }
    ConstIterator begin() const { return data(); }
    ConstIterator end() const { return data() + size(); }
};


#endif // QARRAY_H
