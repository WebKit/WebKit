/****************************************************************************
** $Id$
**
** Implementation of QBuffer class
**
** Created : 930812
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

#ifdef USING_BORROWED_QBUFFER

// -------------------------------------------------------------------------

#include "qbuffer.h"
#include <stdlib.h>

// REVISED: paul
/*!
  \class QBuffer qbuffer.h
  \brief The QBuffer class is an I/O device that operates on a QByteArray

  \ingroup io

  QBuffer allows reading and writing a memory buffer. It is normally
  used together with a QTextStream or a QDataStream.  QBuffer has an
  associated QByteArray which holds the buffer data. The size() of the
  buffer is automatically adjusted as data is written.

  The constructor \c QBuffer(QByteArray) creates a QBuffer with an
  existing byte array.  The byte array can also be set with setBuffer().
  Writing to the QBuffer will modify the original byte array, since
  QByteArray is \link shclass.html explicitly shared.\endlink

  Use open() to open the buffer before use, and to set the mode
  (read-only,write-only, etc.).  close() closes the buffer. The buffer
  must be closed before reopening or calling setBuffer().

  The common way to use QBuffer is through \l QDataStream or \l QTextStream
  which have constructors that take a QBuffer parameter. For
  convenience, there are also QDataStream and QTextStream constructors
  that take a QByteArray parameter.  These constructors create and open
  an internal QBuffer.

  Note that QTextStream can also operate on a QString (a Unicode
  string); a QBuffer cannot.

  You can also use QBuffer directly through the standard QIODevice
  functions readBlock(), writeBlock() readLine(), at(), getch(), putch() and
  ungetch().

  \sa QFile, QDataStream, QTextStream, QByteArray, \link shclass.html Shared Classes\endlink
*/


/*!
  Constructs an empty buffer.
*/

QBuffer::QBuffer()
{
    setFlags( IO_Direct );
    a_inc = 16;					// initial increment
    a_len = 0;
    ioIndex = 0;
}


/*!
  Constructs a buffer that operates on \a buf.
  If you open the buffer in write mode (\c IO_WriteOnly or
  \c IO_ReadWrite) and write something into the buffer, \a buf
  will be modified.


  Example:
  \code
    QCString str = "abc";
    QBuffer b( str );
    b.open( IO_WriteOnly );
    b.at( 3 );					// position at \0
    b.writeBlock( "def", 4 );			// write including \0
    b.close();
      // Now, str == "abcdef"
  \endcode


  \sa setBuffer()
*/

QBuffer::QBuffer( QByteArray buf ) : a(buf)
{
    setFlags( IO_Direct );
    a_len = a.size();
    a_inc = (a_len > 512) ? 512 : a_len;	// initial increment
    if ( a_inc < 16 )
	a_inc = 16;
    ioIndex = 0;
}

/*!
  Destructs the buffer.
*/

QBuffer::~QBuffer()
{
}


/*!
  Replaces the buffer's contents with \a buf.

  This may not be done when isOpen() is TRUE.

  Note that if you open the buffer in write mode (\c IO_WriteOnly or
  IO_ReadWrite) and write something into the buffer, \a buf is also
  modified because QByteArray is an explicitly shared class.

  \sa buffer(), open(), close()
*/

bool QBuffer::setBuffer( QByteArray buf )
{
    if ( isOpen() ) {
#if defined(CHECK_STATE)
	qWarning( "QBuffer::setBuffer: Buffer is open");
#endif
	return FALSE;
    }
    a = buf;
    a_len = a.size();
    a_inc = (a_len > 512) ? 512 : a_len;	// initial increment
    if ( a_inc < 16 )
	a_inc = 16;
    ioIndex = 0;
    return TRUE;
}

/*!
  \fn QByteArray QBuffer::buffer() const

  Returns this buffer's byte array.

  \sa setBuffer()
*/

/*!
  \reimp
  Opens the buffer in the mode \a m.  Returns TRUE if successful,
  otherwise FALSE. The buffer must be opened before use.

  The mode parameter \a m must be a combination of the following flags.
  <ul>
  <li>\c IO_ReadOnly opens a buffer in read-only mode.
  <li>\c IO_WriteOnly opens a buffer in write-only mode.
  <li>\c IO_ReadWrite opens a buffer in read/write mode.
  <li>\c IO_Append sets the buffer index to the end of the buffer.
  <li>\c IO_Truncate truncates the buffer.
  </ul>

  \sa close(), isOpen()
*/

bool QBuffer::open( int m  )
{
    if ( isOpen() ) {				// buffer already open
#if defined(CHECK_STATE)
	qWarning( "QBuffer::open: Buffer already open" );
#endif
	return FALSE;
    }
    setMode( m );
    if ( m & IO_Truncate ) {			// truncate buffer
	a.resize( 0 );
	a_len = 0;
    }
    if ( m & IO_Append ) {			// append to end of buffer
	ioIndex = a.size();
    } else {
	ioIndex = 0;
    }
    a_inc = 16;
    setState( IO_Open );
    setStatus( 0 );
    return TRUE;
}

/*!
  \reimp
  Closes an open buffer.
  \sa open()
*/

void QBuffer::close()
{
    if ( isOpen() ) {
	setFlags( IO_Direct );
	ioIndex = 0;
	a_inc = 16;
    }
}

/*!
  \reimp
  The flush function does nothing for a QBuffer.
*/

void QBuffer::flush()
{
    return;
}


/*!
  \fn int QBuffer::at() const
  \reimp
*/

/*!
  \fn uint QBuffer::size() const
  \reimp
*/

/*!
  \reimp
*/

bool QBuffer::at( int pos )
{
#if defined(CHECK_STATE)
    if ( !isOpen() ) {
	qWarning( "QBuffer::at: Buffer is not open" );
	return FALSE;
    }
#endif
    if ( (uint)pos > a_len ) {
#if defined(CHECK_RANGE)
	qWarning( "QBuffer::at: Index %d out of range", pos );
#endif
	return FALSE;
    }
    ioIndex = pos;
    return TRUE;
}


/*!
  \reimp
*/

int QBuffer::readBlock( char *p, uint len )
{
#if defined(CHECK_STATE)
    CHECK_PTR( p );
    if ( !isOpen() ) {				// buffer not open
	qWarning( "QBuffer::readBlock: Buffer not open" );
	return -1;
    }
    if ( !isReadable() ) {			// reading not permitted
	qWarning( "QBuffer::readBlock: Read operation not permitted" );
	return -1;
    }
#endif
    if ( (uint)ioIndex + len > a.size() ) {	// overflow
	if ( (uint)ioIndex >= a.size() ) {
	    setStatus( IO_ReadError );
	    return -1;
	} else {
	    len = a.size() - (uint)ioIndex;
	}
    }
    memcpy( p, a.data()+ioIndex, len );
    ioIndex += len;
    return len;
}

/*!
  \reimp

  Writes \a len bytes from \a p into the buffer at the current index,
  overwriting any characters there and extending the buffer if necessary.
  Returns the number of bytes actually written.

  Returns -1 if a serious error occurred.

  \sa readBlock()
*/

int QBuffer::writeBlock( const char *p, uint len )
{
#if defined(CHECK_NULL)
    if ( p == 0 && len != 0 )
	qWarning( "QBuffer::writeBlock: Null pointer error" );
#endif
#if defined(CHECK_STATE)
    if ( !isOpen() ) {				// buffer not open
	qWarning( "QBuffer::writeBlock: Buffer not open" );
	return -1;
    }
    if ( !isWritable() ) {			// writing not permitted
	qWarning( "QBuffer::writeBlock: Write operation not permitted" );
	return -1;
    }
#endif
    if ( (uint)ioIndex + len >= a_len ) {		// overflow
	uint new_len = a_len + a_inc*(((uint)ioIndex+len-a_len)/a_inc+1);
	if ( !a.resize( new_len ) ) {		// could not resize
#if defined(CHECK_NULL)
	    qWarning( "QBuffer::writeBlock: Memory allocation error" );
#endif
	    setStatus( IO_ResourceError );
	    return -1;
	}
	a_inc *= 2;				// double increment
	a_len = new_len;
	a.shd->len = (uint)ioIndex + len;
    }
    memcpy( a.data()+ioIndex, p, len );
    ioIndex += len;
    if ( a.shd->len < (uint)ioIndex )
	a.shd->len = (uint)ioIndex;		// fake (not alloc'd) length
    return len;
}


/*!
  \reimp
*/

int QBuffer::readLine( char *p, uint maxlen )
{
#if defined(CHECK_STATE)
    CHECK_PTR( p );
    if ( !isOpen() ) {				// buffer not open
	qWarning( "QBuffer::readLine: Buffer not open" );
	return -1;
    }
    if ( !isReadable() ) {			// reading not permitted
	qWarning( "QBuffer::readLine: Read operation not permitted" );
	return -1;
    }
#endif
    if ( maxlen == 0 )
	return 0;
    uint start = (uint)ioIndex;
    char *d = a.data() + ioIndex;
    maxlen--;					// make room for 0-terminator
    if ( a.size() - (uint)ioIndex < maxlen )
	maxlen = a.size() - (uint)ioIndex;
    while ( maxlen-- ) {
	if ( (*p++ = *d++) == '\n' )
	    break;
    }
    *p = '\0';
    ioIndex = d - a.data();
    return (uint)ioIndex - start;
}


/*!
  \reimp
*/

int QBuffer::getch()
{
#if defined(CHECK_STATE)
    if ( !isOpen() ) {				// buffer not open
	qWarning( "QBuffer::getch: Buffer not open" );
	return -1;
    }
    if ( !isReadable() ) {			// reading not permitted
	qWarning( "QBuffer::getch: Read operation not permitted" );
	return -1;
    }
#endif
    if ( (uint)ioIndex+1 > a.size() ) {		// overflow
	setStatus( IO_ReadError );
	return -1;
    }
    return uchar(*(a.data()+ioIndex++));
}

/*!
  \reimp
  Writes the character \a ch into the buffer, overwriting
  the character at the current index, extending the buffer
  if necessary.

  Returns \a ch, or -1 if some error occurred.

  \sa getch(), ungetch()
*/

int QBuffer::putch( int ch )
{
#if defined(CHECK_STATE)
    if ( !isOpen() ) {				// buffer not open
	qWarning( "QBuffer::putch: Buffer not open" );
	return -1;
    }
    if ( !isWritable() ) {			// writing not permitted
	qWarning( "QBuffer::putch: Write operation not permitted" );
	return -1;
    }
#endif
    if ( (uint)ioIndex + 1 >= a_len ) {		// overflow
	char buf[1];
	buf[0] = (char)ch;
	if ( writeBlock(buf,1) != 1 )
	    return -1;				// write error
    } else {
	*(a.data() + ioIndex++) = (char)ch;
	if ( a.shd->len < (uint)ioIndex )
	    a.shd->len = (uint)ioIndex;
    }
    return ch;
}

/*!
  \reimp
*/

int QBuffer::ungetch( int ch )
{
#if defined(CHECK_STATE)
    if ( !isOpen() ) {				// buffer not open
	qWarning( "QBuffer::ungetch: Buffer not open" );
	return -1;
    }
    if ( !isReadable() ) {			// reading not permitted
	qWarning( "QBuffer::ungetch: Read operation not permitted" );
	return -1;
    }
#endif
    if ( ch != -1 ) {
	if ( ioIndex )
	    ioIndex--;
	else
	    ch = -1;
    }
    return ch;
}

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QBUFFER

// -------------------------------------------------------------------------

