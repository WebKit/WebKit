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

#ifndef _QGARRAY_H_
#define _QGARRAY_H_

// KWQ hacks ---------------------------------------------------------------

#ifndef USING_BORROWED_QARRAY
#define USING_BORROWED_QARRAY
#endif

#include <KWQDef.h>

// -------------------------------------------------------------------------

#include <_qshared.h>

class QGArray					// generic array
{
friend class QBuffer;
public:
    //### DO NOT USE THIS.  IT IS PUBLIC BUT DO NOT USE IT IN NEW CODE.
    struct array_data : public QShared {	// shared array
	array_data()	{ data=0; len=0; }
	char *data;				// actual array data
	uint  len;
    };
    QGArray();
protected:
    QGArray( int, int );			// dummy; does not alloc
    QGArray( int size );			// allocate 'size' bytes
    QGArray( const QGArray &a );		// shallow copy
    virtual ~QGArray();

    QGArray    &operator=( const QGArray &a ) { return assign( a ); }

    virtual void detach()	{ duplicate(*this); }

    char       *data()	 const	{ return shd->data; }
    uint	nrefs()	 const	{ return shd->count; }
    uint	size()	 const	{ return shd->len; }
    bool	isEqual( const QGArray &a ) const;

    bool	resize( uint newsize );

    bool	fill( const char *d, int len, uint sz );

    QGArray    &assign( const QGArray &a );
    QGArray    &assign( const char *d, uint len );
    QGArray    &duplicate( const QGArray &a );
    QGArray    &duplicate( const char *d, uint len );
    void	store( const char *d, uint len );

    array_data *sharedBlock()	const		{ return shd; }
    void	setSharedBlock( array_data *p ) { shd=(array_data*)p; }

    QGArray    &setRawData( const char *d, uint len );
    void	resetRawData( const char *d, uint len );

    int		find( const char *d, uint index, uint sz ) const;
    int		contains( const char *d, uint sz ) const;
    
    void	sort( uint sz );
    int		bsearch( const char *d, uint sz ) const;

    char       *at( uint index ) const;

    bool	setExpand( uint index, const char *d, uint sz );

protected:
    virtual array_data *newData();
    virtual void deleteData( array_data *p );

private:
    static void msg_index( uint );
    array_data *shd;
};


inline char *QGArray::at( uint index ) const
{
#if defined(CHECK_RANGE)
    if ( index >= size() ) {
	msg_index( index );
	index = 0;
    }
#endif
    return &shd->data[index];
}


#endif 
