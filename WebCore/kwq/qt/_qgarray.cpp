/***********************************************************************
 *
 * This file contains temporary implementations of classes from QT/KDE
 *
 * This code is mostly borrowed from QT/KDE and is only modified
 * so it builds and works with KWQ.
 *
 * Eventually we will need to either replace this file, or arrive at
 * some other solution that will enable us to ship this code.
 *
 ***********************************************************************/

// KWQ hacks ---------------------------------------------------------------

#ifndef _KWQ_COMPLETE_
#define _KWQ_COMPLETE_
#endif

// -------------------------------------------------------------------------

#include <_qgarray.h>
#include <stdlib.h>
#include <string.h>

#define USE_MALLOC				// comment to use new/delete

#undef NEW
#undef DELETE

#if defined(USE_MALLOC)
#define NEW(type,size)	((type*)malloc(size*sizeof(type)))
#define DELETE(array)	(free((char*)array))
#else
#define NEW(type,size)	(new type[size])
#define DELETE(array)	(delete[] array)
#define DONT_USE_REALLOC			// comment to use realloc()
#endif


QGArray::QGArray()
{
    shd = newData();
    CHECK_PTR( shd );
}

QGArray::QGArray( int, int )
{
}

QGArray::QGArray( int size )
{
    if ( size < 0 ) {
#if defined(CHECK_RANGE)
	qWarning( "QGArray: Cannot allocate array with negative length" );
#endif
	size = 0;
    }
    shd = newData();
    CHECK_PTR( shd );
    if ( size == 0 )				// zero length
	return;
    shd->data = NEW(char,size);
    CHECK_PTR( shd->data );
    shd->len = size;
}

QGArray::QGArray( const QGArray &a )
{
    shd = a.shd;
    shd->ref();
}

QGArray::~QGArray()
{
    if ( shd && shd->deref() ) {		// delete when last reference
	if ( shd->data )			// is lost
	    DELETE(shd->data);
	deleteData( shd );
    }
}


bool QGArray::isEqual( const QGArray &a ) const
{
    if ( size() != a.size() )			// different size
	return FALSE;
    if ( data() == a.data() )			// has same data
	return TRUE;
    return (size() ? memcmp( data(), a.data(), size() ) : 0) == 0;
}


bool QGArray::resize( uint newsize )
{
    if ( newsize == shd->len )			// nothing to do
	return TRUE;
    if ( newsize == 0 ) {			// remove array
	duplicate( 0, 0 );
	return TRUE;
    }
    if ( shd->data ) {				// existing data
#if defined(DONT_USE_REALLOC)
	char *newdata = NEW(char,newsize);	// manual realloc
	memcpy( newdata, shd->data, QMIN(shd->len,newsize) );
	DELETE(shd->data);
	shd->data = newdata;
#else
	shd->data = (char *)realloc( shd->data, newsize );
#endif
    } else {
	shd->data = NEW(char,newsize);
    }
    CHECK_PTR( shd->data );
    if ( !shd->data )				// no memory
	return FALSE;
    shd->len = newsize;
    return TRUE;
}

bool QGArray::fill( const char *d, int len, uint sz )
{
    if ( len < 0 )
	len = shd->len/sz;			// default: use array length
    else if ( !resize( len*sz ) )
	return FALSE;
    if ( sz == 1 )				// 8 bit elements
	memset( data(), *d, len );
    else if ( sz == 4 ) {			// 32 bit elements
	register Q_INT32 *x = (Q_INT32*)data();
	Q_INT32 v = *((Q_INT32*)d);
	while ( len-- )
	    *x++ = v;
    } else if ( sz == 2 ) {			// 16 bit elements
	register Q_INT16 *x = (Q_INT16*)data();
	Q_INT16 v = *((Q_INT16*)d);
	while ( len-- )
	    *x++ = v;
    } else {					// any other size elements
	register char *x = data();
	while ( len-- ) {			// more complicated
	    memcpy( x, d, sz );
	    x += sz;
	}
    }
    return TRUE;
}

QGArray &QGArray::assign( const QGArray &a )
{
    a.shd->ref();				// avoid 'a = a'
    if ( shd->deref() ) {			// delete when last reference
	if ( shd->data )			// is lost
	    DELETE(shd->data);
	deleteData( shd );
    }
    shd = a.shd;
    return *this;
}

QGArray &QGArray::assign( const char *d, uint len )
{
    if ( shd->count > 1 ) {			// disconnect this
	shd->count--;
	shd = newData();
	CHECK_PTR( shd );
    } else {
	if ( shd->data )
	    DELETE(shd->data);
    }
    shd->data = (char *)d;
    shd->len = len;
    return *this;
}

QGArray &QGArray::duplicate( const QGArray &a )
{
    if ( a.shd == shd ) {			// a.duplicate(a) !
	if ( shd->count > 1 ) {
	    shd->count--;
	    register array_data *n = newData();
	    CHECK_PTR( n );
	    if ( (n->len=shd->len) ) {
		n->data = NEW(char,n->len);
		CHECK_PTR( n->data );
		if ( n->data )
		    memcpy( n->data, shd->data, n->len );
	    } else {
		n->data = 0;
	    }
	    shd = n;
	}
	return *this;
    }
    char *oldptr = 0;
    if ( shd->count > 1 ) {			// disconnect this
	shd->count--;
	shd = newData();
	CHECK_PTR( shd );
    } else {					// delete after copy was made
	oldptr = shd->data;
    }
    if ( a.shd->len ) {				// duplicate data
	shd->data = NEW(char,a.shd->len);
	CHECK_PTR( shd->data );
	if ( shd->data )
	    memcpy( shd->data, a.shd->data, a.shd->len );
    } else {
	shd->data = 0;
    }
    shd->len = a.shd->len;
    if ( oldptr )
	DELETE(oldptr);
    return *this;
}

QGArray &QGArray::duplicate( const char *d, uint len )
{
    char *data;
    if ( d == 0 || len == 0 ) {
	data = 0;
	len  = 0;
    } else {
	if ( shd->count == 1 && shd->len == len ) {
	    memcpy( shd->data, d, len );	// use same buffer
	    return *this;
	}
	data = NEW(char,len);
	CHECK_PTR( data );
	memcpy( data, d, len );
    }
    if ( shd->count > 1 ) {			// detach
	shd->count--;
	shd = newData();
	CHECK_PTR( shd );
    } else {					// just a single reference
	if ( shd->data )
	    DELETE(shd->data);
    }
    shd->data = data;
    shd->len  = len;
    return *this;
}

void QGArray::store( const char *d, uint len )
{						// store, but not deref
    resize( len );
    memcpy( shd->data, d, len );
}

QGArray &QGArray::setRawData( const char *d, uint len )
{
    duplicate( 0, 0 );				// set null data
    shd->data = (char *)d;
    shd->len  = len;
    return *this;
}

void QGArray::resetRawData( const char *d, uint len )
{
    if ( d != shd->data || len != shd->len ) {
	return;
    }
    shd->data = 0;
    shd->len  = 0;
}

int QGArray::find( const char *d, uint index, uint sz ) const
{
    index *= sz;
    if ( index >= shd->len ) {
	return -1;
    }
    register uint i;
    uint ii;
    switch ( sz ) {
	case 1: {				// 8 bit elements
	    register char *x = data() + index;
	    char v = *d;
	    for ( i=index; i<shd->len; i++ ) {
		if ( *x++ == v )
		    break;
	    }
	    ii = i;
	    }
	    break;
	case 2: {				// 16 bit elements
	    register Q_INT16 *x = (Q_INT16*)(data() + index);
	    Q_INT16 v = *((Q_INT16*)d);
	    for ( i=index; i<shd->len; i+=2 ) {
		if ( *x++ == v )
		    break;
	    }
	    ii = i/2;
	    }
	    break;
	case 4: {				// 32 bit elements
	    register Q_INT32 *x = (Q_INT32*)(data() + index);
	    Q_INT32 v = *((Q_INT32*)d);
	    for ( i=index; i<shd->len; i+=4 ) {
		if ( *x++ == v )
		    break;
	    }
	    ii = i/4;
	    }
	    break;
	default: {				// any size elements
	    for ( i=index; i<shd->len; i+=sz ) {
		if ( memcmp( d, &shd->data[i], sz ) == 0 )
		    break;
	    }
	    ii = i/sz;
	    }
	    break;
    }
    return i<shd->len ? (int)ii : -1;
}

int QGArray::contains( const char *d, uint sz ) const
{
    register uint i = shd->len;
    int count = 0;
    switch ( sz ) {
	case 1: {				// 8 bit elements
	    register char *x = data();
	    char v = *d;
	    while ( i-- ) {
		if ( *x++ == v )
		    count++;
	    }
	    }
	    break;
	case 2: {				// 16 bit elements
	    register Q_INT16 *x = (Q_INT16*)data();
	    Q_INT16 v = *((Q_INT16*)d);
	    i /= 2;
	    while ( i-- ) {
		if ( *x++ == v )
		    count++;
	    }
	    }
	    break;
	case 4: {				// 32 bit elements
	    register Q_INT32 *x = (Q_INT32*)data();
	    Q_INT32 v = *((Q_INT32*)d);
	    i /= 4;
	    while ( i-- ) {
		if ( *x++ == v )
		    count++;
	    }
	    }
	    break;
	default: {				// any size elements
	    for ( i=0; i<shd->len; i+=sz ) {
		if ( memcmp(d, &shd->data[i], sz) == 0 )
		    count++;
	    }
	    }
	    break;
    }
    return count;
}

static int cmp_item_size = 0;

#if defined(Q_C_CALLBACKS)
extern "C" {
#endif

static int cmp_arr( const void *n1, const void *n2 )
{
    return ( n1 && n2 ) ? memcmp( n1, n2, cmp_item_size ) 
	                : (int)((long)n1 - (long)n2);
    // Qt 3.0: Add a virtual compareItems() method and call that instead
}

#if defined(Q_C_CALLBACKS)
}
#endif

void QGArray::sort( uint sz )
{
    int numItems = size() / sz;
    if ( numItems < 2 )
	return;
    cmp_item_size = sz;
    qsort( shd->data, numItems, sz, cmp_arr );
}

int QGArray::bsearch( const char *d, uint sz ) const
{
    int numItems = size() / sz;
    if ( !numItems )
	return -1;
    cmp_item_size = sz;
    char* r = (char*)::bsearch( d, shd->data, numItems, sz, cmp_arr );
    if ( !r )
	return -1;
    while( (r >= shd->data + sz) && (cmp_arr( r - sz, d ) == 0) )
	r -= sz;	// search to first of equal elements; bsearch is undef 
    return (int)(( r - shd->data ) / sz);
}

bool QGArray::setExpand( uint index, const char *d, uint sz )
{
    index *= sz;
    if ( index >= shd->len ) {
	if ( !resize( index+sz ) )		// no memory
	    return FALSE;
    }
    memcpy( data() + index, d, sz );
    return TRUE;
}

void QGArray::msg_index( uint index )
{
#if defined(CHECK_RANGE)
    qWarning( "QGArray::at: Absolute index %d out of range", index );
#else
    Q_UNUSED( index )
#endif
}

QGArray::array_data * QGArray::newData()
{
    return new array_data;
}

void QGArray::deleteData( array_data *p )
{
    delete p;
    p = 0;
}
