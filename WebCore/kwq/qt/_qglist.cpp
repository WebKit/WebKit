/****************************************************************************
** $Id$
**
** Implementation of QGList and QGListIterator classes
**
** Created : 920624
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

// KWQ hacks ---------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USING_BORROWED_QLIST

// -------------------------------------------------------------------------

#include "_qglist.h"
#include "_qgvector.h"
#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif


// NOT REVISED
/*!
  \class QLNode qglist.h
  \brief The QLNode class is an internal class for the QList template collection.

  QLNode is a doubly linked list node; it has three pointers:
  <ol>
  <li> Pointer to the previous node.
  <li> Pointer to the next node.
  <li> Pointer to the actual data.
  </ol>

  Sometimes it might be practical to have direct access to the list nodes
  in a QList, but it is seldom required.

  \warning Be very careful if you want to access the list nodes. The heap
  can easily get corrupted if you make a mistake.

  \sa QList::currentNode(), QList::removeNode(), QList::takeNode()
*/

/*!
  \fn QCollection::Item QLNode::getData()
  Returns a pointer (\c void*) to the actual data in the list node.
*/


/*!
  \class QGList qglist.h
  \brief The QGList class is an internal class for implementing Qt collection classes.

  QGList is a strictly internal class that acts as a base class for several
  \link collection.html collection classes\endlink; QList, QQueue and
  QStack.

  QGList has some virtual functions that can be reimplemented to customize
  the subclasses.
  <ul>
  <li> compareItems() compares two collection/list items.
  <li> read() reads a collection/list item from a QDataStream.
  <li> write() writes a collection/list item to a QDataStream.
  </ul>
  Normally, you do not have to reimplement any of these functions.
  If you still want to reimplement them, see the QStrList class (qstrlist.h),
  which is a good example.
*/


/*****************************************************************************
  Default implementation of virtual functions
 *****************************************************************************/

/*!
  This virtual function compares two list items.

  Returns:
  <ul>
  <li> 0 if \e item1 == \e item2
  <li> non-zero if \e item1 != \e item2
  </ul>

  This function returns \e int rather than \e bool so that
  reimplementations can return three values and use it to sort by:

  <ul>
  <li> 0 if \e item1 == \e item2
  <li> \> 0 (positive integer) if \e item1 \> \e item2
  <li> \< 0 (negative integer) if \e item1 \< \e item2
  </ul>

  The QList::inSort() function requires that compareItems() is implemented
  as described here.

  This function should not modify the list because some const functions
  call compareItems().

  The default implementation compares the pointers:
  \code

  \endcode
*/

int QGList::compareItems( QCollection::Item item1, QCollection::Item item2 )
{
    return item1 != item2;			// compare pointers
}

#ifndef QT_NO_DATASTREAM
/*!
  Reads a collection/list item from the stream \a s and returns a reference
  to the stream.

  The default implementation sets \a item to 0.

  \sa write()
*/

QDataStream &QGList::read( QDataStream &s, QCollection::Item &item )
{
    item = 0;
    return s;
}

/*!
  Writes a collection/list item to the stream \a s and returns a reference
  to the stream.

  The default implementation does nothing.

  \sa read()
*/

QDataStream &QGList::write( QDataStream &s, QCollection::Item ) const
{
    return s;
}
#endif // QT_NO_DATASTREAM

/*****************************************************************************
  QGList member functions
 *****************************************************************************/

/*!
  \internal
  Constructs an empty list.
*/

QGList::QGList()
{
    firstNode = lastNode = curNode = 0;		// initialize list
    numNodes  = 0;
    curIndex  = -1;
    iterators = 0;				// initialize iterator list
}

/*!
  \internal
  Constructs a copy of \e list.
*/

QGList::QGList( const QGList & list )
    : QCollection( list )
{
    firstNode = lastNode = curNode = 0;		// initialize list
    numNodes  = 0;
    curIndex  = -1;
    iterators = 0;				// initialize iterator list
    QLNode *n = list.firstNode;
    while ( n ) {				// copy all items from list
	append( n->data );
	n = n->next;
    }
}

/*!
  \internal
  Removes all items from the list and destroys the list.
*/

QGList::~QGList()
{
    clear();
    if ( !iterators )				// no iterators for this list
	return;
    QGListIterator *i = (QGListIterator*)iterators->first();
    while ( i ) {				// notify all iterators that
	i->list = 0;				// this list is deleted
	i->curNode = 0;
	i = (QGListIterator*)iterators->next();
    }
    delete iterators;
}


/*!
  \internal
  Assigns \e list to this list.
*/

QGList& QGList::operator=( const QGList &list )
{
    clear();
    if ( list.count() > 0 ) {
	QLNode *n = list.firstNode;
	while ( n ) {				// copy all items from list
	    append( n->data );
	    n = n->next;
	}
	curNode	 = firstNode;
	curIndex = 0;
    }
    return *this;
}

/*!
  Compares this list with \a list. Retruns TRUE if the lists
  contain the same data, else FALSE.
*/

bool QGList::operator==( const QGList &list ) const
{
    if ( count() != list.count() )
	return FALSE;
    
    if ( count() == 0 )
	return TRUE;
    
    QLNode *n1 = firstNode;
    QLNode *n2 = list.firstNode;
    while ( n1 && n2 ) {
	// should be mutable
	if ( ( (QGList*)this )->compareItems( n1->data, n2->data ) != 0 )
	    return FALSE;
	n1 = n1->next;
	n2 = n2->next;
    }
    
    return TRUE;
}

/*!
  \fn uint QGList::count() const
  \internal
  Returns the number of items in the list.
*/


/*!
  \internal
  Returns the node at position \e index.  Sets this node to current.
*/

QLNode *QGList::locate( uint index )
{
    if ( index == (uint)curIndex )		// current node ?
	return curNode;
    if ( !curNode && firstNode ) {		// set current node
	curNode	 = firstNode;
	curIndex = 0;
    }
    register QLNode *node;
    int	 distance = index - curIndex;		// node distance to cur node
    bool forward;				// direction to traverse

    if ( index >= numNodes ) {
#if defined(CHECK_RANGE)
	qWarning( "QGList::locate: Index %d out of range", index );
#endif
	return 0;
    }

    if ( distance < 0 )
	distance = -distance;
    if ( (uint)distance < index && (uint)distance < numNodes - index ) {
	node =	curNode;			// start from current node
	forward = index > (uint)curIndex;
    } else if ( index < numNodes - index ) {	// start from first node
	node = firstNode;
	distance = index;
	forward = TRUE;
    } else {					// start from last node
	node = lastNode;
	distance = numNodes - index - 1;
	if ( distance < 0 )
	    distance = 0;
	forward = FALSE;
    }
    if ( forward ) {				// now run through nodes
	while ( distance-- )
	    node = node->next;
    } else {
	while ( distance-- )
	    node = node->prev;
    }
    curIndex = index;				// must update index
    return curNode = node;
}


/*!
  \internal
  Inserts an item at its sorted position in the list.
*/

void QGList::inSort( QCollection::Item d )
{
    int index = 0;
    register QLNode *n = firstNode;
    while ( n && compareItems(n->data,d) < 0 ){ // find position in list
	n = n->next;
	index++;
    }
    insertAt( index, d );
}


/*!
  \internal
  Inserts an item at the start of the list.
*/

void QGList::prepend( QCollection::Item d )
{
    register QLNode *n = new QLNode( newItem(d) );
    CHECK_PTR( n );
    n->prev = 0;
    if ( (n->next = firstNode) )		// list is not empty
	firstNode->prev = n;
    else					// initialize list
	lastNode = n;
    firstNode = curNode = n;			// curNode affected
    numNodes++;
    curIndex = 0;
}


/*!
  \internal
  Inserts an item at the end of the list.
*/

void QGList::append( QCollection::Item d )
{
    register QLNode *n = new QLNode( newItem(d) );
    CHECK_PTR( n );
    n->next = 0;
    if ( (n->prev = lastNode) )			// list is not empty
	lastNode->next = n;
    else					// initialize list
	firstNode = n;
    lastNode = curNode = n;			// curNode affected
    curIndex = numNodes;
    numNodes++;
}


/*!
  \internal
  Inserts an item at position \e index in the list.
*/

bool QGList::insertAt( uint index, QCollection::Item d )
{
    if ( index == 0 ) {				// insert at head of list
	prepend( d );
	return TRUE;
    } else if ( index == numNodes ) {		// append at tail of list
	append( d );
	return TRUE;
    }
    QLNode *nextNode = locate( index );
    if ( !nextNode )				// illegal position
	return FALSE;
    QLNode *prevNode = nextNode->prev;
    register QLNode *n = new QLNode( newItem(d) );
    CHECK_PTR( n );
    nextNode->prev = n;
    prevNode->next = n;
    n->prev = prevNode;				// link new node into list
    n->next = nextNode;
    curNode = n;				// curIndex set by locate()
    numNodes++;
    return TRUE;
}


/*!
  \internal
  Relinks node \e n and makes it the first node in the list.
*/

void QGList::relinkNode( QLNode *n )
{
    if ( n == firstNode )			// already first
	return;
    curNode = n;
    unlink();
    n->prev = 0;
    if ( (n->next = firstNode) )		// list is not empty
	firstNode->prev = n;
    else					// initialize list
	lastNode = n;
    firstNode = curNode = n;			// curNode affected
    numNodes++;
    curIndex = 0;
}


/*!
  \internal
  Unlinks the current list node and returns a pointer to this node.
*/

QLNode *QGList::unlink()
{
    if ( curNode == 0 )				// null current node
	return 0;
    register QLNode *n = curNode;		// unlink this node
    if ( n == firstNode ) {			// removing first node ?
	if ( (firstNode = n->next) ) {
	    firstNode->prev = 0;
	} else {
	    lastNode = curNode = 0;		// list becomes empty
	    curIndex = -1;
	}
    } else {
	if ( n == lastNode ) {			// removing last node ?
	    lastNode = n->prev;
	    lastNode->next = 0;
	} else {				// neither last nor first node
	    n->prev->next = n->next;
	    n->next->prev = n->prev;
	}
    }
    if ( n->next ) {				// change current node
	curNode = n->next;
    } else if ( n->prev ) {
	curNode = n->prev;
	curIndex--;
    }
    if ( iterators && iterators->count() ) {	// update iterators
	QGListIterator *i = (QGListIterator*)iterators->first();
	while ( i ) {				// fix all iterators that
	    if ( i->curNode == n )		// refers to pending node
		i->curNode = curNode;
	    i = (QGListIterator*)iterators->next();
	}
    }
    numNodes--;
    return n;
}


/*!
  \internal
  Removes the node \e n from the list.
*/

bool QGList::removeNode( QLNode *n )
{
#if defined(CHECK_NULL)
    if ( n == 0 || (n->prev && n->prev->next != n) ||
	 (n->next && n->next->prev != n) ) {
	qWarning( "QGList::removeNode: Corrupted node" );
	return FALSE;
    }
#endif
    curNode = n;
    unlink();					// unlink node
    deleteItem( n->data );			// deallocate this node
    delete n;
    curNode  = firstNode;
    curIndex = curNode ? 0 : -1;
    return TRUE;
}

/*!
  \internal
  Removes the item \e d from the list.	Uses compareItems() to find the item.
*/

bool QGList::remove( QCollection::Item d )
{
    if ( d ) {					// find the item
	if ( find(d) == -1 )
	    return FALSE;
    }
    QLNode *n = unlink();			// unlink node
    if ( !n )
	return FALSE;
    deleteItem( n->data );			// deallocate this node
    delete n;
    return TRUE;
}

/*!
  \internal
  Removes the item \e d from the list.
*/

bool QGList::removeRef( QCollection::Item d )
{
    if ( d ) {					// find the item
	if ( findRef(d) == -1 )
	    return FALSE;
    }
    QLNode *n = unlink();			// unlink node
    if ( !n )
	return FALSE;
    deleteItem( n->data );			// deallocate this node
    delete n;
    return TRUE;
}

/*!
  \fn bool QGList::removeFirst()
  \internal
  Removes the first item in the list.
*/

/*!
  \fn bool QGList::removeLast()
  \internal
  Removes the last item in the list.
*/

/*!
  \internal
  Removes the item at position \e index from the list.
*/

bool QGList::removeAt( uint index )
{
    if ( !locate(index) )
	return FALSE;
    QLNode *n = unlink();			// unlink node
    if ( !n )
	return FALSE;
    deleteItem( n->data );			// deallocate this node
    delete n;
    return TRUE;
}


/*!
  \internal
  Takes the node \e n out of the list.
*/

QCollection::Item QGList::takeNode( QLNode *n )
{
#if defined(CHECK_NULL)
    if ( n == 0 || (n->prev && n->prev->next != n) ||
	 (n->next && n->next->prev != n) ) {
	qWarning( "QGList::takeNode: Corrupted node" );
	return 0;
    }
#endif
    curNode = n;
    unlink();					// unlink node
    Item d = n->data;
    delete n;					// delete the node, not data
    curNode  = firstNode;
    curIndex = curNode ? 0 : -1;
    return d;
}

/*!
  \internal
  Takes the current item out of the list.
*/

QCollection::Item QGList::take()
{
    QLNode *n = unlink();			// unlink node
    Item d = n ? n->data : 0;
    delete n;					// delete node, keep contents
    return d;
}

/*!
  \internal
  Takes the item at position \e index out of the list.
*/

QCollection::Item QGList::takeAt( uint index )
{
    if ( !locate(index) )
	return 0;
    QLNode *n = unlink();			// unlink node
    Item d = n ? n->data : 0;
    delete n;					// delete node, keep contents
    return d;
}

/*!
  \internal
  Takes the first item out of the list.
*/

QCollection::Item QGList::takeFirst()
{
    first();
    QLNode *n = unlink();			// unlink node
    Item d = n ? n->data : 0;
    delete n;
    return d;
}

/*!
  \internal
  Takes the last item out of the list.
*/

QCollection::Item QGList::takeLast()
{
    last();
    QLNode *n = unlink();			// unlink node
    Item d = n ? n->data : 0;
    delete n;
    return d;
}


/*!
  \internal
  Removes all items from the list.
*/

void QGList::clear()
{
    register QLNode *n = firstNode;

    firstNode = lastNode = curNode = 0;		// initialize list
    numNodes = 0;
    curIndex = -1;

    if ( iterators && iterators->count() ) {
	QGListIterator *i = (QGListIterator*)iterators->first();
	while ( i ) {				// notify all iterators that
	    i->curNode = 0;			// this list is empty
	    i = (QGListIterator*)iterators->next();
	}
    }

    QLNode *prevNode;
    while ( n ) {				// for all nodes ...
	deleteItem( n->data );			// deallocate data
	prevNode = n;
	n = n->next;
	delete prevNode;			// deallocate node
    }
}


/*!
  \internal
  Finds an item in the list.
*/

int QGList::findRef( QCollection::Item d, bool fromStart )
{
    register QLNode *n;
    int	     index;
    if ( fromStart ) {				// start from first node
	n = firstNode;
	index = 0;
    } else {					// start from current node
	n = curNode;
	index = curIndex;
    }
    while ( n && n->data != d ) {		// find exact match
	n = n->next;
	index++;
    }
    curNode = n;
    curIndex = n ? index : -1;
    return curIndex;				// return position of item
}

/*!
  \internal
  Finds an item in the list.  Uses compareItems().
*/

int QGList::find( QCollection::Item d, bool fromStart )
{
    register QLNode *n;
    int	     index;
    if ( fromStart ) {				// start from first node
	n = firstNode;
	index = 0;
    } else {					// start from current node
	n = curNode;
	index = curIndex;
    }
    while ( n && compareItems(n->data,d) ){	// find equal match
	n = n->next;
	index++;
    }
    curNode = n;
    curIndex = n ? index : -1;
    return curIndex;				// return position of item
}


/*!
  \internal
  Counts the number an item occurs in the list.
*/

uint QGList::containsRef( QCollection::Item d ) const
{
    register QLNode *n = firstNode;
    uint     count = 0;
    while ( n ) {				// for all nodes...
	if ( n->data == d )			// count # exact matches
	    count++;
	n = n->next;
    }
    return count;
}

/*!
  \internal
  Counts the number an item occurs in the list.	 Uses compareItems().
*/

uint QGList::contains( QCollection::Item d ) const
{
    register QLNode *n = firstNode;
    uint     count = 0;
    QGList  *that = (QGList*)this;		// mutable for compareItems()
    while ( n ) {				// for all nodes...
	if ( !that->compareItems(n->data,d) )	// count # equal matches
	    count++;
	n = n->next;
    }
    return count;
}


/*!
  \fn QCollection::Item QGList::at( uint index )
  \internal
  Sets the item at position \e index to the current item.
*/

/*!
  \fn int QGList::at() const
  \internal
  Returns the current index.
*/

/*!
  \fn QLNode *QGList::currentNode() const
  \internal
  Returns the current node.
*/

/*!
  \fn QCollection::Item QGList::get() const
  \internal
  Returns the current item.
*/

/*!
  \fn QCollection::Item QGList::cfirst() const
  \internal
  Returns the first item in the list.
*/

/*!
  \fn QCollection::Item QGList::clast() const
  \internal
  Returns the last item in the list.
*/


/*!
  \internal
  Returns the first list item.	Sets this to current.
*/

QCollection::Item QGList::first()
{
    if ( firstNode ) {
	curIndex = 0;
	return (curNode=firstNode)->data;
    }
    return 0;
}

/*!
  \internal
  Returns the last list item.  Sets this to current.
*/

QCollection::Item QGList::last()
{
    if ( lastNode ) {
	curIndex = numNodes-1;
	return (curNode=lastNode)->data;
    }
    return 0;
}

/*!
  \internal
  Returns the next list item (after current).  Sets this to current.
*/

QCollection::Item QGList::next()
{
    if ( curNode ) {
	if ( curNode->next ) {
	    curIndex++;
	    curNode = curNode->next;
	    return curNode->data;
	}
	curIndex = -1;
	curNode = 0;
    }
    return 0;
}

/*!
  \internal
  Returns the previous list item (before current).  Sets this to current.
*/

QCollection::Item QGList::prev()
{
    if ( curNode ) {
	if ( curNode->prev ) {
	    curIndex--;
	    curNode = curNode->prev;
	    return curNode->data;
	}
	curIndex = -1;
	curNode = 0;
    }
    return 0;
}


/*!
  \internal
  Converts the list to a vector.
*/

void QGList::toVector( QGVector *vector ) const
{
    vector->clear();
    if ( !vector->resize( count() ) )
	return;
    register QLNode *n = firstNode;
    uint i = 0;
    while ( n ) {
	vector->insert( i, n->data );
	n = n->next;
	i++;
    }
}

void QGList::heapSortPushDown( QCollection::Item* heap, int first, int last )
{
    int r = first;
    while( r <= last/2 ) {
	// Node r has only one child ?
	if ( last == 2*r ) {
	    // Need for swapping ?
	    if ( compareItems( heap[r], heap[ 2*r ] ) > 0 ) {
		QCollection::Item tmp = heap[r];
		heap[ r ] = heap[ 2*r ];
		heap[ 2*r ] = tmp;
	    }
	    // That's it ...
	    r = last;
	} else {
	    // Node has two children
	    if ( compareItems( heap[r], heap[ 2*r ] ) > 0 &&
		 compareItems( heap[ 2*r ], heap[ 2*r+1 ] ) <= 0 ) {
		// Swap with left child
		QCollection::Item tmp = heap[r];
		heap[ r ] = heap[ 2*r ];
		heap[ 2*r ] = tmp;
		r *= 2;
	    } else if ( compareItems( heap[r], heap[ 2*r+1 ] ) > 0 &&
			compareItems( heap[ 2*r+1 ], heap[ 2*r ] ) < 0 ) {
		// Swap with right child
		QCollection::Item tmp = heap[r];
		heap[ r ] = heap[ 2*r+1 ];
		heap[ 2*r+1 ] = tmp;
		r = 2*r+1;
	    } else {
		// We are done
		r = last;
	    }
	}
    }
}


/*! Sorts the list by the result of the virtual compareItems() function.

  The Heap-Sort algorithm is used for sorting.  It sorts n items with
  O(n*log n) compares.  This is the asymptotic optimal solution of the
  sorting problem.
*/

void QGList::sort()
{
    uint n = count();
    if ( n < 2 )
	return;

    // Create the heap
    QCollection::Item* realheap = new QCollection::Item[ n ];
    // Wow, what a fake. But I want the heap to be indexed as 1...n
    QCollection::Item* heap = realheap - 1;
    int size = 0;
    QLNode* insert = firstNode;
    for( ; insert != 0; insert = insert->next ) {
	heap[++size] = insert->data;
	int i = size;
	while( i > 1 && compareItems( heap[i], heap[ i / 2 ] ) < 0 ) {
	    QCollection::Item tmp = heap[ i ];
	    heap[ i ] = heap[ i/2 ];
	    heap[ i/2 ] = tmp;
	    i /= 2;
	}
    }

    insert = firstNode;
    // Now do the sorting
    for ( int i = n; i > 0; i-- ) {
	insert->data = heap[1];
	insert = insert->next;
	if ( i > 1 ) {
	    heap[1] = heap[i];
	    heapSortPushDown( heap, 1, i - 1 );
	}
    }

    delete [] realheap;
}


/*****************************************************************************
  QGList stream functions
 *****************************************************************************/

#ifndef QT_NO_DATASTREAM
QDataStream &operator>>( QDataStream &s, QGList &list )
{						// read list
    return list.read( s );
}

QDataStream &operator<<( QDataStream &s, const QGList &list )
{						// write list
    return list.write( s );
}

/*!
  \internal
  Reads a list from the stream \e s.
*/

QDataStream &QGList::read( QDataStream &s )
{
    uint num;
    s >> num;					// read number of items
    clear();					// clear list
    while ( num-- ) {				// read all items
	Item d;
	read( s, d );
	CHECK_PTR( d );
	if ( !d )				// no memory
	    break;
	QLNode *n = new QLNode( d );
	CHECK_PTR( n );
	if ( !n )				// no memory
	    break;
	n->next = 0;
	if ( (n->prev = lastNode) )		// list is not empty
	    lastNode->next = n;
	else					// initialize list
	    firstNode = n;
	lastNode = n;
	numNodes++;
    }
    curNode  = firstNode;
    curIndex = curNode ? 0 : -1;
    return s;
}

/*!
  \internal
  Writes the list to the stream \e s.
*/

QDataStream &QGList::write( QDataStream &s ) const
{
    s << count();				// write number of items
    QLNode *n = firstNode;
    while ( n ) {				// write all items
	write( s, n->data );
	n = n->next;
    }
    return s;
}

#endif // QT_NO_DATASTREAM

/*****************************************************************************
  QGListIterator member functions
 *****************************************************************************/

/*!
  \class QGListIterator qglist.h
  \brief The QGListIterator class is an internal class for implementing QListIterator.

  QGListIterator is a strictly internal class that does the heavy work for
  QListIterator.
*/

/*!
  \internal
  Constructs an iterator that operates on the list \e l.
*/

QGListIterator::QGListIterator( const QGList &l )
{
    list = (QGList *)&l;			// get reference to list
    curNode = list->firstNode;			// set to first node
    if ( !list->iterators ) {
	list->iterators = new QGList;		// create iterator list
	CHECK_PTR( list->iterators );
    }
    list->iterators->append( this );		// attach iterator to list
}

/*!
  \internal
  Constructs a copy of the iterator \e it.
*/

QGListIterator::QGListIterator( const QGListIterator &it )
{
    list = it.list;
    curNode = it.curNode;
    if ( list )
	list->iterators->append( this );	// attach iterator to list
}

/*!
  \internal
  Assigns a copy of the iterator \e it and returns a reference to this
  iterator.
*/

QGListIterator &QGListIterator::operator=( const QGListIterator &it )
{
    if ( list )					// detach from old list
	list->iterators->removeRef( this );
    list = it.list;
    curNode = it.curNode;
    if ( list )
	list->iterators->append( this );	// attach to new list
    return *this;
}

/*!
  \internal
  Destroys the iterator.
*/

QGListIterator::~QGListIterator()
{
    if ( list )					// detach iterator from list
	list->iterators->removeRef(this);
}


/*!
  \fn bool QGListIterator::atFirst() const
  \internal
  Returns TRUE if the iterator points to the first item, otherwise FALSE.
*/

/*!
  \fn bool QGListIterator::atLast() const
  \internal
  Returns TRUE if the iterator points to the last item, otherwise FALSE.
*/


/*!
  \internal
  Sets the list iterator to point to the first item in the list.
*/

QCollection::Item QGListIterator::toFirst()
{
    if ( !list ) {
#if defined(CHECK_NULL)
	qWarning( "QGListIterator::toFirst: List has been deleted" );
#endif
	return 0;
    }
    return list->firstNode ? (curNode = list->firstNode)->getData() : 0;
}

/*!
  \internal
  Sets the list iterator to point to the last item in the list.
*/

QCollection::Item QGListIterator::toLast()
{
    if ( !list ) {
#if defined(CHECK_NULL)
	qWarning( "QGListIterator::toLast: List has been deleted" );
#endif
	return 0;
    }
    return list->lastNode ? (curNode = list->lastNode)->getData() : 0;
}


/*!
  \fn QCollection::Item QGListIterator::get() const
  \internal
  Returns the iterator item.
*/


/*!
  \internal
  Moves to the next item (postfix).
*/

QCollection::Item QGListIterator::operator()()
{
    if ( !curNode )
	return 0;
    QCollection::Item d = curNode->getData();
    curNode = curNode->next;
    return  d;
}

/*!
  \internal
  Moves to the next item (prefix).
*/

QCollection::Item QGListIterator::operator++()
{
    if ( !curNode )
	return 0;
    curNode = curNode->next;
    return curNode ? curNode->getData() : 0;
}

/*!
  \internal
  Moves \e jumps positions forward.
*/

QCollection::Item QGListIterator::operator+=( uint jumps )
{
    while ( curNode && jumps-- )
	curNode = curNode->next;
    return curNode ? curNode->getData() : 0;
}

/*!
  \internal
  Moves to the previous item (prefix).
*/

QCollection::Item QGListIterator::operator--()
{
    if ( !curNode )
	return 0;
    curNode = curNode->prev;
    return curNode ? curNode->getData() : 0;
}

/*!
  \internal
  Moves \e jumps positions backward.
*/

QCollection::Item QGListIterator::operator-=( uint jumps )
{
    while ( curNode && jumps-- )
	curNode = curNode->prev;
    return curNode ? curNode->getData() : 0;
}

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QVECTOR

// -------------------------------------------------------------------------
