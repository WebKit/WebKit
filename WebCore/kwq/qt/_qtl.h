/****************************************************************************
** $Id$
**
** Definition of Qt template library classes
**
** Created : 990128
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
#ifndef QTL_H
#define QTL_H

// KWQ hacks ---------------------------------------------------------------

#ifndef _KWQ_COMPLETE_
#define _KWQ_COMPLETE_
#endif

#include <KWQDef.h>

// -------------------------------------------------------------------------

#ifndef QT_H

#ifndef QT_NO_TEXTSTREAM
#include "qtextstream.h"
#endif

#include "qstring.h"
#endif // QT_H

#ifndef QT_NO_TEXTSTREAM
template <class T>
class QTextOStreamIterator
{
protected:
    QTextOStream& stream;
    QString separator;

public:
    QTextOStreamIterator( QTextOStream& s) : stream( s ) {}
    QTextOStreamIterator( QTextOStream& s, const QString& sep )
	: stream( s ), separator( sep )  {}
    QTextOStreamIterator<T>& operator= ( const T& x ) {
	stream << x;
	if ( !separator.isEmpty() )
	    stream << separator;
	return *this;
    }
    QTextOStreamIterator<T>& operator*() { return *this; }
    QTextOStreamIterator<T>& operator++() { return *this; }
    QTextOStreamIterator<T>& operator++(int) { return *this; }
};
#endif //QT_NO_TEXTSTREAM

template <class InputIterator, class OutputIterator>
inline OutputIterator qCopy( InputIterator _begin, InputIterator _end,
			     OutputIterator _dest )
{
    while( _begin != _end )
	*_dest++ = *_begin++;
    return _dest;
}


template <class T>
inline void qSwap( T& _value1, T& _value2 )
{
    T tmp = _value1;
    _value1 = _value2;
    _value2 = tmp;
}


template <class InputIterator>
inline void qBubbleSort( InputIterator b, InputIterator e )
{
    // Goto last element;
    InputIterator last = e;
    --last;
    // only one element or no elements ?
    if ( last == b )
	return;

    // So we have at least two elements in here
    while( b != last ) {
	bool swapped = FALSE;
	InputIterator swap_pos = b;
	InputIterator x = e;
	InputIterator y = x;
	y--;
	do {
	    --x;
	    --y;
	    if ( *x < *y ) {
		swapped = TRUE;
		qSwap( *x, *y );
		swap_pos = y;
	    }
	} while( y != b );
	if ( !swapped )
	    return;
	b = swap_pos;
	b++;
    }
}


template <class Container>
inline void qBubbleSort( Container &c )
{
  qBubbleSort( c.begin(), c.end() );
}


template <class Value>
inline void qHeapSortPushDown( Value* heap, int first, int last )
{
    int r = first;
    while( r <= last/2 ) {
	// Node r has only one child ?
	if ( last == 2*r ) {
	    // Need for swapping ?
	    if ( heap[r] > heap[ 2*r ] )
		qSwap( heap[r], heap[ 2*r ] );
	    // That's it ...
	    r = last;
	} else { // Node has two children
	    if ( heap[r] > heap[ 2*r ] && heap[ 2*r ] <= heap[ 2*r+1 ] ) {
		// Swap with left child
		qSwap( heap[r], heap[ 2*r ] );
		r *= 2;
	    } else if ( heap[r] > heap[ 2*r+1 ] &&
			heap[ 2*r+1 ] < heap[ 2*r ] ) {
		// Swap with right child
		qSwap( heap[r], heap[ 2*r+1 ] );
		r = 2*r+1;
	    } else {
		// We are done
		r = last;
	    }
	}
    }
}


template <class InputIterator, class Value>
inline void qHeapSortHelper( InputIterator b, InputIterator e, Value, uint n )
{
    // Create the heap
    InputIterator insert = b;
    Value* realheap = new Value[ n ];
    // Wow, what a fake. But I want the heap to be indexed as 1...n
    Value* heap = realheap - 1;
    int size = 0;
    for( ; insert != e; ++insert ) {
	heap[++size] = *insert;
	int i = size;
	while( i > 1 && heap[i] < heap[ i / 2 ] ) {
	    qSwap( heap[i], heap[ i / 2 ] );
	    i /= 2;
	}
    }

    // Now do the sorting
    for( uint i = n; i > 0; i-- ) {
	*b++ = heap[1];
	if ( i > 1 ) {
	    heap[1] = heap[i];
	    qHeapSortPushDown( heap, 1, (int)i - 1 );
	}
    }

    delete[] realheap;
}


template <class InputIterator>
inline void qHeapSort( InputIterator b, InputIterator e )
{
    // Empty ?
    if ( b == e )
	return;

    // How many entries have to be sorted ?
    InputIterator it = b;
    uint n = 0;
    while ( it != e ) {
	++n;
	++it;
    }

    // The second last parameter is a hack to retrieve the value type
    // Do the real sorting here
    qHeapSortHelper( b, e, *b, n );
}


template <class Container>
inline void qHeapSort( Container &c )
{
    if ( c.isEmpty() )
	return;

    // The second last parameter is a hack to retrieve the value type
    // Do the real sorting here
    qHeapSortHelper( c.begin(), c.end(), *(c.begin()), c.count() );
}

#endif
