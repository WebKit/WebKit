/****************************************************************************
** $Id$
**
** Definition of QGVector class
**
** Created : 930907
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

#ifndef QGVECTOR_H
#define QGVECTOR_H

// KWQ hacks ---------------------------------------------------------------

#ifndef USING_BORROWED_QVECTOR
#define USING_BORROWED_QVECTOR
#endif

#include <KWQDef.h>

// -------------------------------------------------------------------------

#ifndef QT_H
#include "_qcollection.h"
#endif // QT_H


class Q_EXPORT QGVector : public QCollection	// generic vector
{
friend class QGList;				// needed by QGList::toVector
public:
#ifndef QT_NO_DATASTREAM
    QDataStream &read( QDataStream & );		// read vector from stream
    QDataStream &write( QDataStream & ) const;	// write vector to stream
#endif
    virtual int compareItems( Item, Item );

protected:
    QGVector();					// create empty vector
    QGVector( uint size );			// create vector with nullptrs
    QGVector( const QGVector &v );		// make copy of other vector
   ~QGVector();

    QGVector &operator=( const QGVector &v );	// assign from other vector

    Item	 *data()    const	{ return vec; }
    uint  size()    const	{ return len; }
    uint  count()   const	{ return numItems; }

    bool  insert( uint index, Item );		// insert item at index
    bool  remove( uint index );			// remove item
    Item	  take( uint index );			// take out item

    void  clear();				// clear vector
    bool  resize( uint newsize );		// resize vector

    bool  fill( Item, int flen );		// resize and fill vector

    void  sort();				// sort vector
    int	  bsearch( Item ) const;			// binary search (when sorted)

    int	  findRef( Item, uint index ) const;	// find exact item in vector
    int	  find( Item, uint index ) const;	// find equal item in vector
    uint  containsRef( Item ) const;		// get number of exact matches
    uint  contains( Item ) const;		// get number of equal matches

    Item	  at( uint index ) const		// return indexed item
    {
#if defined(CHECK_RANGE)
	if ( index >= len )
	    warningIndexRange( index );
#endif
	return vec[index];
    }

    bool insertExpand( uint index, Item );	// insert, expand if necessary

    void toList( QGList * ) const;		// put items in list

#ifndef QT_NO_DATASTREAM
    virtual QDataStream &read( QDataStream &, Item & );
    virtual QDataStream &write( QDataStream &, Item ) const;
#endif
private:
    Item	 *vec;
    uint  len;
    uint  numItems;

    static void warningIndexRange( uint );
};


/*****************************************************************************
  QGVector stream functions
 *****************************************************************************/

#ifndef QT_NO_DATASTREAM
Q_EXPORT QDataStream &operator>>( QDataStream &, QGVector & );
Q_EXPORT QDataStream &operator<<( QDataStream &, const QGVector & );
#endif

#endif // QGVECTOR_H
