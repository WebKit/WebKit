/****************************************************************************
** $Id$
**
** Implementation of QGDict and QGDictIterator classes
**
** Created : 920529
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

#ifdef USING_BORROWED_QDICT

// -------------------------------------------------------------------------

#include "_qgdict.h"
#include "qlist.h"
#include "qstring.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif

#include <ctype.h>


// NOT REVISED
/*!
  \class QGDict qgdict.h
  \brief The QGDict class is an internal class for implementing QDict template classes.

  QGDict is a strictly internal class that acts as a base class for the
  \link collection.html collection classes\endlink QDict and QIntDict.

  QGDict has some virtual functions that can be reimplemented to customize
  the subclasses.
  <ul>
  <li> read() reads a collection/dictionary item from a QDataStream.
  <li> write() writes a collection/dictionary item to a QDataStream.
  </ul>
  Normally, you do not have to reimplement any of these functions.
*/

static const int op_find    = 0;
static const int op_insert  = 1;
static const int op_replace = 2;


class QGDItList : public QList<QGDictIterator>
{
public:
    QGDItList() : QList<QGDictIterator>() {}
    QGDItList( const QGDItList &list ) : QList<QGDictIterator>(list) {}
   ~QGDItList() { clear(); }
    QGDItList &operator=(const QGDItList &list)
	{ return (QGDItList&)QList<QGDictIterator>::operator=(list); }
};


/*****************************************************************************
  Default implementation of special and virtual functions
 *****************************************************************************/

/*!
  \internal
  Returns the hash key for \e key, when key is a string.
*/

int QGDict::hashKeyString( const QString &key )
{
#if defined(CHECK_NULL)
    if ( key.isNull() )
	qWarning( "QGDict::hashStringKey: Invalid null key" );
#endif
#ifdef USING_BORROWED_QSTRING
    int i;
    register uint h=0;
    uint g;
    const QChar *p = key.unicode();
    if ( cases ) {				// case sensitive
	for ( i=0; i<(int)key.length(); i++ ) {
	    h = (h<<4) + p[i].cell();
	    if ( (g = h & 0xf0000000) )
		h ^= g >> 24;
	    h &= ~g;
	}
    } else {					// case insensitive
	for ( i=0; i<(int)key.length(); i++ ) {
	    h = (h<<4) + p[i].lower().cell();
	    if ( (g = h & 0xf0000000) )
		h ^= g >> 24;
	    h &= ~g;
	}
    }
    int index = h;
    if ( index < 0 )				// adjust index to table size
	index = -index;
    return index;
#else
    return CFHash(key.s);
#endif
}

/*!
  \internal
  Returns the hash key for \a key, which is a C string.
*/

int QGDict::hashKeyAscii( const char *key )
{
#if defined(CHECK_NULL)
    if ( key == 0 )
	qWarning( "QGDict::hashAsciiKey: Invalid null key" );
#endif
    register const char *k = key;
    register uint h=0;
    uint g;
    if ( cases ) {				// case sensitive
	while ( *k ) {
	    h = (h<<4) + *k++;
	    if ( (g = h & 0xf0000000) )
		h ^= g >> 24;
	    h &= ~g;
	}
    } else {					// case insensitive
	while ( *k ) {
	    h = (h<<4) + tolower(*k);
	    if ( (g = h & 0xf0000000) )
		h ^= g >> 24;
	    h &= ~g;
	    k++;
	}
    }
    int index = h;
    if ( index < 0 )				// adjust index to table size
	index = -index;
    return index;
}

#ifndef QT_NO_DATASTREAM

/*!
  Reads a collection/dictionary item from the stream \e s and returns a
  reference to the stream.

  The default implementation sets \e item to 0.

  \sa write()
*/

QDataStream& QGDict::read( QDataStream &s, QCollection::Item &item )
{
    item = 0;
    return s;
}

/*!
  Writes a collection/dictionary item to the stream \e s and returns a
  reference to the stream.

  \sa read()
*/

QDataStream& QGDict::write( QDataStream &s, QCollection::Item ) const
{
    return s;
}
#endif //QT_NO_DATASTREAM

/*****************************************************************************
  QGDict member functions
 *****************************************************************************/

/*!
  \internal
  Constructs a dictionary.
*/

QGDict::QGDict( uint len, KeyType kt, bool caseSensitive, bool copyKeys )
{
    init( len, kt, caseSensitive, copyKeys );
}


void QGDict::init( uint len, KeyType kt, bool caseSensitive, bool copyKeys )
{
    vec = new QBaseBucket *[vlen = len];		// allocate hash table
    CHECK_PTR( vec );
    memset( (char*)vec, 0, vlen*sizeof(QBaseBucket*) );
    numItems  = 0;
    iterators = 0;
    // The caseSensitive and copyKey options don't make sense for
    // all dict types.
    switch ( (keytype = (uint)kt) ) {
	case StringKey:
	    cases = caseSensitive;
	    copyk = FALSE;
	    break;
	case AsciiKey:
	    cases = caseSensitive;
	    copyk = copyKeys;
	    break;
	default:
	    cases = FALSE;
	    copyk = FALSE;
	    break;
    }
}


/*!
  \internal
  Constructs a copy of \e dict.
*/

QGDict::QGDict( const QGDict & dict )
    : QCollection( dict )
{
    init( dict.vlen, (KeyType)dict.keytype, dict.cases, dict.copyk );
    QGDictIterator it( dict );
    while ( it.get() ) {			// copy from other dict
	switch ( keytype ) {
	    case StringKey:
		look_string( it.getKeyString(), it.get(), op_insert );
		break;
	    case AsciiKey:
		look_ascii( it.getKeyAscii(), it.get(), op_insert );
		break;
	    case IntKey:
		look_int( it.getKeyInt(), it.get(), op_insert );
		break;
	    case PtrKey:
		look_ptr( it.getKeyPtr(), it.get(), op_insert );
		break;
	}
	++it;
    }
}


/*!
  \internal
  Removes all items from the dictionary and destroys it.
*/

QGDict::~QGDict()
{
    clear();					// delete everything
    delete [] vec;
    if ( !iterators )				// no iterators for this dict
	return;
    QGDictIterator *i = iterators->first();
    while ( i ) {				// notify all iterators that
	i->dict = 0;				// this dict is deleted
	i = iterators->next();
    }
    delete iterators;
}


/*!
  \internal
  Assigns \e dict to this dictionary.
*/

QGDict &QGDict::operator=( const QGDict &dict )
{
    clear();
    QGDictIterator it( dict );
    while ( it.get() ) {			// copy from other dict
	switch ( keytype ) {
	    case StringKey:
		look_string( it.getKeyString(), it.get(), op_insert );
		break;
	    case AsciiKey:
		look_ascii( it.getKeyAscii(), it.get(), op_insert );
		break;
	    case IntKey:
		look_int( it.getKeyInt(), it.get(), op_insert );
		break;
	    case PtrKey:
		look_ptr( it.getKeyPtr(), it.get(), op_insert );
		break;
	}
	++it;
    }
    return *this;
}


/*! \fn QCollection::Item QGDictIterator::get() const

  \internal
*/


/*! \fn QString QGDictIterator::getKeyString() const

  \internal
*/


/*! \fn const char * QGDictIterator::getKeyAscii() const

  \internal
*/


/*! \fn void * QGDictIterator::getKeyPtr() const

  \internal
*/


/*! \fn long QGDictIterator::getKeyInt() const

  \internal
*/


/*!
  \fn uint QGDict::count() const
  \internal
  Returns the number of items in the dictionary.
*/

/*!
  \fn uint QGDict::size() const
  \internal
  Returns the size of the hash array.
*/


/*!
  \internal
  The do-it-all function; op is one of op_find, op_insert, op_replace
*/

QCollection::Item QGDict::look_string( const QString &key, QCollection::Item d, int op )
{
    QStringBucket *n;
    int	index = hashKeyString(key) % vlen;
    if ( op == op_find ) {			// find
	if ( cases ) {
	    for ( n=(QStringBucket*)vec[index]; n;
		  n=(QStringBucket*)n->getNext() ) {
		if ( key == n->getKey() )
		    return n->getData();	// item found
	    }
	} else {
	    QString k = key.lower();
	    for ( n=(QStringBucket*)vec[index]; n;
		  n=(QStringBucket*)n->getNext() ) {
		if ( k == n->getKey().lower() )
		    return n->getData();	// item found
	    }
	}
	return 0;				// not found
    }
    if ( op == op_replace ) {			// replace
	if ( vec[index] != 0 )			// maybe something there
	    remove_string( key );
    }
    // op_insert or op_replace
    n = new QStringBucket(key,newItem(d),vec[index]);
    CHECK_PTR( n );
#if defined(CHECK_NULL)
    if ( n->getData() == 0 )
	qWarning( "QDict: Cannot insert null item" );
#endif
    vec[index] = n;
    numItems++;
    return n->getData();
}


/*!  \internal */

QCollection::Item QGDict::look_ascii( const char *key, QCollection::Item d, int op )
{
    QAsciiBucket *n;
    int	index = hashKeyAscii(key) % vlen;
    if ( op == op_find ) {			// find
	if ( cases ) {
	    for ( n=(QAsciiBucket*)vec[index]; n;
		  n=(QAsciiBucket*)n->getNext() ) {
		if ( qstrcmp(n->getKey(),key) == 0 )
		    return n->getData();	// item found
	    }
	} else {
	    for ( n=(QAsciiBucket*)vec[index]; n;
		  n=(QAsciiBucket*)n->getNext() ) {
		if ( qstricmp(n->getKey(),key) == 0 )
		    return n->getData();	// item found
	    }
	}
	return 0;				// not found
    }
    if ( op == op_replace ) {			// replace
	if ( vec[index] != 0 )			// maybe something there
	    remove_ascii( key );
    }
    // op_insert or op_replace
    n = new QAsciiBucket(copyk ? qstrdup(key) : key,newItem(d),vec[index]);
    CHECK_PTR( n );
#if defined(CHECK_NULL)
    if ( n->getData() == 0 )
	qWarning( "QAsciiDict: Cannot insert null item" );
#endif
    vec[index] = n;
    numItems++;
    return n->getData();
}


/*!  \internal */

QCollection::Item QGDict::look_int( long key, QCollection::Item d, int op )
{
    QIntBucket *n;
    int index = (int)((ulong)key % vlen);	// simple hash
    if ( op == op_find ) {			// find
	for ( n=(QIntBucket*)vec[index]; n;
	      n=(QIntBucket*)n->getNext() ) {
	    if ( n->getKey() == key )
		return n->getData();		// item found
	}
	return 0;				// not found
    }
    if ( op == op_replace ) {			// replace
	if ( vec[index] != 0 )			// maybe something there
	    remove_int( key );
    }
    // op_insert or op_replace
    n = new QIntBucket(key,newItem(d),vec[index]);
    CHECK_PTR( n );
#if defined(CHECK_NULL)
    if ( n->getData() == 0 )
	qWarning( "QIntDict: Cannot insert null item" );
#endif
    vec[index] = n;
    numItems++;
    return n->getData();
}


/*!  \internal */

QCollection::Item QGDict::look_ptr( void *key, QCollection::Item d, int op )
{
    QPtrBucket *n;
    int index = (int)((ulong)key % vlen);	// simple hash
    if ( op == op_find ) {			// find
	for ( n=(QPtrBucket*)vec[index]; n;
	      n=(QPtrBucket*)n->getNext() ) {
	    if ( n->getKey() == key )
		return n->getData();		// item found
	}
	return 0;				// not found
    }
    if ( op == op_replace ) {			// replace
	if ( vec[index] != 0 )			// maybe something there
	    remove_ptr( key );
    }
    // op_insert or op_replace
    n = new QPtrBucket(key,newItem(d),vec[index]);
    CHECK_PTR( n );
#if defined(CHECK_NULL)
    if ( n->getData() == 0 )
	qWarning( "QPtrDict: Cannot insert null item" );
#endif
    vec[index] = n;
    numItems++;
    return n->getData();
}


/*!
  \internal
  Changes the size of the hashtable.
  The contents of the dictionary are preserved,
  but all iterators on the dictionary become invalid.
*/
void QGDict::resize( uint newsize )
{
    // Save old information
    QBaseBucket **old_vec = vec;
    uint old_vlen  = vlen;
    bool old_copyk = copyk;

    vec = new QBaseBucket *[vlen = newsize];
    CHECK_PTR( vec );
    memset( (char*)vec, 0, vlen*sizeof(QBaseBucket*) );
    numItems = 0;
    copyk = FALSE;

    // Reinsert every item from vec, deleting vec as we go
    for ( uint index = 0; index < old_vlen; index++ ) {
	switch ( keytype ) {
	    case StringKey:
		{
		    QStringBucket *n=(QStringBucket *)old_vec[index];
		    while ( n ) {
			look_string( n->getKey(), n->getData(), op_insert );
			QStringBucket *t=(QStringBucket *)n->getNext();
			delete n;
			n = t;
		    }
		}
		break;
	    case AsciiKey:
		{
		    QAsciiBucket *n=(QAsciiBucket *)old_vec[index];
		    while ( n ) {
			look_ascii( n->getKey(), n->getData(), op_insert );
			QAsciiBucket *t=(QAsciiBucket *)n->getNext();
			delete n;
			n = t;
		    }
		}
		break;
	    case IntKey:
		{
		    QIntBucket *n=(QIntBucket *)old_vec[index];
		    while ( n ) {
			look_int( n->getKey(), n->getData(), op_insert );
			QIntBucket *t=(QIntBucket *)n->getNext();
			delete n;
			n = t;
		    }
		}
		break;
	    case PtrKey:
		{
		    QPtrBucket *n=(QPtrBucket *)old_vec[index];
		    while ( n ) {
			look_ptr( n->getKey(), n->getData(), op_insert );
			QPtrBucket *t=(QPtrBucket *)n->getNext();
			delete n;
			n = t;
		    }
		}
		break;
	}
    }
    delete [] old_vec;

    // Restore state
    copyk = old_copyk;

    // Invalidate all iterators, since order is lost
    if ( iterators && iterators->count() ) {
	QGDictIterator *i = iterators->first();
	while ( i ) {
	    i->toFirst();
	    i = iterators->next();
	}
    }
}

/*!
  \internal
  Unlinks the bucket with the specified key (and specified data pointer,
  if it is set).
*/

void QGDict::unlink_common( int index, QBaseBucket *node, QBaseBucket *prev )
{
    if ( iterators && iterators->count() ) {	// update iterators
	QGDictIterator *i = iterators->first();
	while ( i ) {				// invalidate all iterators
	    if ( i->curNode == node )		// referring to pending node
		i->operator++();
	    i = iterators->next();
	}
    }
    if ( prev )					// unlink node
	prev->setNext( node->getNext() );
    else
	vec[index] = node->getNext();
    numItems--;
}

QStringBucket *QGDict::unlink_string( const QString &key, QCollection::Item d )
{
    if ( numItems == 0 )			// nothing in dictionary
	return 0;
    QStringBucket *n;
    QStringBucket *prev = 0;
    int index = hashKeyString(key) % vlen;
    if ( cases ) {
	for ( n=(QStringBucket*)vec[index]; n;
	      n=(QStringBucket*)n->getNext() ) {
	    bool found = (key == n->getKey());
	    if ( found && d )
		found = (n->getData() == d);
	    if ( found ) {
		unlink_common(index,n,prev);
		return n;
	    }
	    prev = n;
	}
    } else {
	QString k = key.lower();
	for ( n=(QStringBucket*)vec[index]; n;
	      n=(QStringBucket*)n->getNext() ) {
	    bool found = (k == n->getKey().lower());
	    if ( found && d )
		found = (n->getData() == d);
	    if ( found ) {
		unlink_common(index,n,prev);
		return n;
	    }
	    prev = n;
	}
    }
    return 0;
}

QAsciiBucket *QGDict::unlink_ascii( const char *key, QCollection::Item d )
{
    if ( numItems == 0 )			// nothing in dictionary
	return 0;
    QAsciiBucket *n;
    QAsciiBucket *prev = 0;
    int index = hashKeyAscii(key) % vlen;	
    for ( n=(QAsciiBucket *)vec[index]; n; n=(QAsciiBucket *)n->getNext() ) {
	bool found = (cases ? qstrcmp(n->getKey(),key)
		       : qstricmp(n->getKey(),key)) == 0;
	if ( found && d )
	    found = (n->getData() == d);
	if ( found ) {
	    unlink_common(index,n,prev);
	    return n;
	}
	prev = n;
    }
    return 0;
}

QIntBucket *QGDict::unlink_int( long key, QCollection::Item d )
{
    if ( numItems == 0 )			// nothing in dictionary
	return 0;
    QIntBucket *n;
    QIntBucket *prev = 0;
    int index = (int)((ulong)key % vlen);
    for ( n=(QIntBucket *)vec[index]; n; n=(QIntBucket *)n->getNext() ) {
	bool found = (n->getKey() == key);
	if ( found && d )
	    found = (n->getData() == d);
	if ( found ) {
	    unlink_common(index,n,prev);
	    return n;
	}
	prev = n;
    }
    return 0;
}

QPtrBucket *QGDict::unlink_ptr( void *key, QCollection::Item d )
{
    if ( numItems == 0 )			// nothing in dictionary
	return 0;
    QPtrBucket *n;
    QPtrBucket *prev = 0;
    int index = (int)((ulong)key % vlen);
    for ( n=(QPtrBucket *)vec[index]; n; n=(QPtrBucket *)n->getNext() ) {
	bool found = (n->getKey() == key);
	if ( found && d )
	    found = (n->getData() == d);
	if ( found ) {
	    unlink_common(index,n,prev);
	    return n;
	}
	prev = n;
    }
    return 0;
}


/*!
  \internal
  Removes the item with the specified key.  If item is non-null,
  the remove will match the \a item as well (used to remove an
  item when several items have the same key).
*/

bool QGDict::remove_string( const QString &key, QCollection::Item item )
{
    QStringBucket *n = unlink_string( key, item );
    if ( n ) {
	deleteItem( n->getData() );
	delete n;
	return TRUE;
    } else {
	return FALSE;
    }
}


/*!  \internal */

bool QGDict::remove_ascii( const char *key, QCollection::Item item )
{
    QAsciiBucket *n = unlink_ascii( key, item );
    if ( n ) {
	if ( copyk )
	    delete [] (char *)n->getKey();
	deleteItem( n->getData() );
	delete n;
    }
    return n != 0;
}


/*!  \internal */

bool QGDict::remove_int( long key, QCollection::Item item )
{
    QIntBucket *n = unlink_int( key, item );
    if ( n ) {
	deleteItem( n->getData() );
	delete n;
    }
    return n != 0;
}


/*!  \internal */

bool QGDict::remove_ptr( void *key, QCollection::Item item )
{
    QPtrBucket *n = unlink_ptr( key, item );
    if ( n ) {
	deleteItem( n->getData() );
	delete n;
    }
    return n != 0;
}


/*!  \internal */

QCollection::Item QGDict::take_string( const QString &key )
{
    QStringBucket *n = unlink_string( key );
    Item d;
    if ( n ) {
	d = n->getData();
	delete n;
    } else {
	d = 0;
    }
    return d;
}


/*!  \internal */

QCollection::Item QGDict::take_ascii( const char *key )
{
    QAsciiBucket *n = unlink_ascii( key );
    Item d;
    if ( n ) {
	if ( copyk )
	    delete [] (char *)n->getKey();
	d = n->getData();
	delete n;
    } else {
	d = 0;
    }
    return d;
}


/*!  \internal */

QCollection::Item QGDict::take_int( long key )
{
    QIntBucket *n = unlink_int( key );
    Item d;
    if ( n ) {
	d = n->getData();
	delete n;
    } else {
	d = 0;
    }
    return d;
}


/*!  \internal */

QCollection::Item QGDict::take_ptr( void *key )
{
    QPtrBucket *n = unlink_ptr( key );
    Item d;
    if ( n ) {
	d = n->getData();
	delete n;
    } else {
	d = 0;
    }
    return d;
}


/*!
  \internal
  Removes all items from the dictionary.
*/

void QGDict::clear()
{
    if ( !numItems )
	return;
    numItems = 0;				// disable remove() function
    for ( uint j=0; j<vlen; j++ ) {		// destroy hash table
	if ( vec[j] ) {
	    switch ( keytype ) {
		case StringKey:
		    {
			QStringBucket *n=(QStringBucket *)vec[j];
			while ( n ) {
			    QStringBucket *next = (QStringBucket*)n->getNext();
			    deleteItem( n->getData() );
			    delete n;
			    n = next;
			}
		    }
		    break;
		case AsciiKey:
		    {
			QAsciiBucket *n=(QAsciiBucket *)vec[j];
			while ( n ) {
			    QAsciiBucket *next = (QAsciiBucket*)n->getNext();
			    if ( copyk )
				delete [] (char *)n->getKey();
			    deleteItem( n->getData() );
			    delete n;
			    n = next;
			}
		    }
		    break;
		case IntKey:
		    {
			QIntBucket *n=(QIntBucket *)vec[j];
			while ( n ) {
			    QIntBucket *next = (QIntBucket*)n->getNext();
			    deleteItem( n->getData() );
			    delete n;
			    n = next;
			}
		    }
		    break;
		case PtrKey:
		    {
			QPtrBucket *n=(QPtrBucket *)vec[j];
			while ( n ) {
			    QPtrBucket *next = (QPtrBucket*)n->getNext();
			    deleteItem( n->getData() );
			    delete n;
			    n = next;
			}
		    }
		    break;
	    }
	    vec[j] = 0;				// detach list of buckets
	}
    }
    if ( iterators && iterators->count() ) {	// invalidate all iterators
	QGDictIterator *i = iterators->first();
	while ( i ) {
	    i->curNode = 0;
	    i = iterators->next();
	}
    }
}


/*!
  \internal
  Outputs debug statistics.
*/

void QGDict::statistics() const
{
#if defined(DEBUG)
    QString line;
    line.fill( '-', 60 );
    double real, ideal;
    qDebug( line.ascii() );
    qDebug( "DICTIONARY STATISTICS:" );
    if ( count() == 0 ) {
	qDebug( "Empty!" );
	qDebug( line.ascii() );
	return;
    }
    real = 0.0;
    ideal = (float)count()/(2.0*size())*(count()+2.0*size()-1);
    uint i = 0;
    while ( i<size() ) {
	QBaseBucket *n = vec[i];
	int b = 0;
	while ( n ) {				// count number of buckets
	    b++;
	    n = n->getNext();
	}
	real = real + (double)b * ((double)b+1.0)/2.0;
	char buf[80], *pbuf;
	if ( b > 78 )
	    b = 78;
	pbuf = buf;
	while ( b-- )
	    *pbuf++ = '*';
	*pbuf = '\0';
	qDebug( buf );
	i++;
    }
    qDebug( "Array size = %d", size() );
    qDebug( "# items    = %d", count() );
    qDebug( "Real dist  = %g", real );
    qDebug( "Rand dist  = %g", ideal );
    qDebug( "Real/Rand  = %g", real/ideal );
    qDebug( line.ascii() );
#endif // DEBUG
}


/*****************************************************************************
  QGDict stream functions
 *****************************************************************************/
#ifndef QT_NO_DATASTREAM
QDataStream &operator>>( QDataStream &s, QGDict &dict )
{
    return dict.read( s );
}

QDataStream &operator<<( QDataStream &s, const QGDict &dict )
{
    return dict.write( s );
}

#if defined(_CC_DEC_) && defined(__alpha) && (__DECCXX_VER >= 50190001)
#pragma message disable narrowptr
#endif

/*!
  \internal
  Reads a dictionary from the stream \e s.
*/

QDataStream &QGDict::read( QDataStream &s )
{
    uint num;
    s >> num;					// read number of items
    clear();					// clear dict
    while ( num-- ) {				// read all items
	Item d;
	switch ( keytype ) {
	    case StringKey:
		{
		    QString k;
		    s >> k;
		    read( s, d );
		    look_string( k, d, op_insert );
		}
		break;
	    case AsciiKey:
		{
		    char *k;
		    s >> k;
		    read( s, d );
		    look_ascii( k, d, op_insert );
		    if ( copyk )
			delete [] k;
		}
		break;
	    case IntKey:
		{
		    Q_UINT32 k;
		    s >> k;
		    read( s, d );
		    look_int( k, d, op_insert );
		}
		break;
	    case PtrKey:
		{
		    Q_UINT32 k;
		    s >> k;
		    read( s, d );
		    // ### cannot insert 0 - this renders the thing
		    // useless since all pointers are written as 0,
		    // but hey, serializing pointers?  can it be done
		    // at all, ever?
		    if ( k )
			look_ptr( (void *)k, d, op_insert );
		}
		break;
	}
    }
    return s;
}

/*!
  \internal
  Writes the dictionary to the stream \e s.
*/

QDataStream& QGDict::write( QDataStream &s ) const
{
    s << count();				// write number of items
    uint i = 0;
    while ( i<size() ) {
	QBaseBucket *n = vec[i];
	while ( n ) {				// write all buckets
	    switch ( keytype ) {
		case StringKey:
		    s << ((QStringBucket*)n)->getKey();
		    break;
		case AsciiKey:
		    s << ((QAsciiBucket*)n)->getKey();
		    break;
		case IntKey:
		    s << (Q_UINT32)((QIntBucket*)n)->getKey();
		    break;
		case PtrKey:
		    s << (Q_UINT32)0; // ### cannot serialize a pointer
		    break;
	    }
	    write( s, n->getData() );		// write data
	    n = n->getNext();
	}
	i++;
    }
    return s;
}
#endif //QT_NO_DATASTREAM

/*****************************************************************************
  QGDictIterator member functions
 *****************************************************************************/

/*!
  \class QGDictIterator qgdict.h
  \brief An internal class for implementing QDictIterator and QIntDictIterator.

  QGDictIterator is a strictly internal class that does the heavy work for
  QDictIterator and QIntDictIterator.
*/

/*!
  \internal
  Constructs an iterator that operates on the dictionary \e d.
*/

QGDictIterator::QGDictIterator( const QGDict &d )
{
    dict = (QGDict *)&d;			// get reference to dict
    toFirst();					// set to first noe
    if ( !dict->iterators ) {
	dict->iterators = new QGDItList;	// create iterator list
	CHECK_PTR( dict->iterators );
    }
    dict->iterators->append( this );		// attach iterator to dict
}

/*!
  \internal
  Constructs a copy of the iterator \e it.
*/

QGDictIterator::QGDictIterator( const QGDictIterator &it )
{
    dict = it.dict;
    curNode = it.curNode;
    curIndex = it.curIndex;
    if ( dict )
	dict->iterators->append( this );	// attach iterator to dict
}

/*!
  \internal
  Assigns a copy of the iterator \e it and returns a reference to this
  iterator.
*/

QGDictIterator &QGDictIterator::operator=( const QGDictIterator &it )
{
    if ( dict )					// detach from old dict
	dict->iterators->removeRef( this );
    dict = it.dict;
    curNode = it.curNode;
    curIndex = it.curIndex;
    if ( dict )
	dict->iterators->append( this );	// attach to new list
    return *this;
}

/*!
  \internal
  Destroys the iterator.
*/

QGDictIterator::~QGDictIterator()
{
    if ( dict )					// detach iterator from dict
	dict->iterators->removeRef( this );
}


/*!
  \internal
  Sets the iterator to point to the first item in the dictionary.
*/

QCollection::Item QGDictIterator::toFirst()
{
    if ( !dict ) {
#if defined(CHECK_NULL)
	qWarning( "QGDictIterator::toFirst: Dictionary has been deleted" );
#endif
	return 0;
    }
    if ( dict->count() == 0 ) {			// empty dictionary
	curNode = 0;
	return 0;
    }
    register uint i = 0;
    register QBaseBucket **v = dict->vec;
    while ( !(*v++) )
	i++;
    curNode = dict->vec[i];
    curIndex = i;
    return curNode->getData();
}


/*!
  \internal
  Moves to the next item (postfix).
*/

QCollection::Item QGDictIterator::operator()()
{
    if ( !dict ) {
#if defined(CHECK_NULL)
	qWarning( "QGDictIterator::operator(): Dictionary has been deleted" );
#endif
	return 0;
    }
    if ( !curNode )
	return 0;
    QCollection::Item d = curNode->getData();
    this->operator++();
    return d;
}

/*!
  \internal
  Moves to the next item (prefix).
*/

QCollection::Item QGDictIterator::operator++()
{
    if ( !dict ) {
#if defined(CHECK_NULL)
	qWarning( "QGDictIterator::operator++: Dictionary has been deleted" );
#endif
	return 0;
    }
    if ( !curNode )
	return 0;
    curNode = curNode->getNext();
    if ( !curNode ) {				// no next bucket
	register uint i = curIndex + 1;		// look from next vec element
	register QBaseBucket **v = &dict->vec[i];
	while ( i < dict->size() && !(*v++) )
	    i++;
	if ( i == dict->size() ) {		// nothing found
	    curNode = 0;
	    return 0;
	}
	curNode = dict->vec[i];
	curIndex = i;
    }
    return curNode->getData();
}

/*!
  \internal
  Moves \e jumps positions forward.
*/

QCollection::Item QGDictIterator::operator+=( uint jumps )
{
    while ( curNode && jumps-- )
	operator++();
    return curNode ? curNode->getData() : 0;
}

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QDICT

// -------------------------------------------------------------------------
