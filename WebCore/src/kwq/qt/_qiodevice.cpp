/****************************************************************************
** $Id$
**
** Implementation of QIODevice class
**
** Created : 940913
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

#include "_qiodevice.h"

// NOT REVISED
/*!
  \class QIODevice qiodevice.h

  \brief The QIODevice class is the base class of I/O devices.

  \ingroup io

  An I/O device represents a medium that one can read bytes from
  and/or write bytes to.  The QIODevice class is the abstract
  superclass of all such devices; classes like QFile, QBuffer and
  QSocket inherit QIODevice and implement virtual functions like
  write() appropriately.

  While applications sometimes use QIODevice directly, mostly it is
  better to go through QTextStream and QDataStream, which provide
  stream operations on any QIODevice subclass.  QTextStream provides
  text-oriented stream functionality (for human-readable ASCII files,
  for example), while QDataStream deals with binary data in a totally
  platform-independent manner.

  The public member functions in QIODevice roughly fall into two
  groups: The action functions and the state access functions.  The
  most important action functions are: <ul>

  <li> open() opens a device for reading and/or writing, depending on
  the argument to open().

  <li> close() closes the device and tidies up.

  <li> readBlock() reads a block of data from the device.

  <li> writeBlock() writes a block of data to the device.

  <li> readLine() reads a line (of text, usually) from the device.

  <li> flush() ensures that all buffered data are written to the real device.

  </ul>There are also some other, less used, action functions: <ul>

  <li> getch() reads a single character.

  <li> ungetch() forgets the last call to getch(), if possible.

  <li> putch() writes a single character.

  <li> size() returns the size of the device, if there is one.

  <li> at() returns the current read/write pointer, if there is one
  for this device, or it moves the pointer.

  <li> atEnd() says whether there is more to read, if that is a
  meaningful question for this device.

  <li> reset() moves the read/write pointer to the start of the
  device, if that is possible for this device.

  </ul>The state access are all "get" functions.  The QIODevice subclass
  calls setState() to update the state, and simple access functions
  tell the user of the device what the device's state is.  Here are
  the settings, and their associated access functions: <ul>

  <li> Access type.  Some devices are direct access (it is possible to
  read/write anywhere) while others are sequential.  QIODevice
  provides the access functions isDirectAccess(), isSequentialAccess()
  and isCombinedAccess() to tell users what a given I/O device
  supports.

  <li> Buffering.  Some devices are accessed in raw mode while others
  are buffered.  Buffering usually provides greater efficiency,
  particularly for small read/write operations.  isBuffered() tells
  the user whether a given device is buffered.  (This can often be set
  by the application in the call to open().)

  <li> Synchronicity.  Synchronous devices work there and then, for
  example files.  When you read from a file, the file delivers its
  data right away.  Others, such as a socket connected to a HTTP
  server, may not deliver the data until seconds after you ask to read
  it.  isSynchronous() and isAsynchronous() tells the user how this
  device operates.

  <li> CR/LF translation.  For simplicity, applications often like to
  see just a single CR/LF style, and QIODevice subclasses can provide
  that.  isTranslated() returns TRUE if this object translates CR/LF
  to just LF.  (This can often be set by the application in the call
  to open().)

  <li> Accessibility.  Some files cannot be written, for example.
  isReadable(), isWritable and isReadWrite() tells the application
  whether it can read from and write to a given device.  (This can
  often be set by the application in the call to open().)

  <li> Finally, isOpen() returns TRUE if the device is open.  This can
  quite obviously be set using open() :)

  </ul>

  QIODevice provides numerous pure virtual functions you need to
  implement when subclassing it.  Here is a skeleton subclass with all
  the members you are certain to need, and some it's likely that you
  will need:

  \code
    class YourDevice : public QIODevice
    {
    public:
	YourDevice();
       ~YourDevice();

	bool open( int mode );
	void close();
	void flush();

	uint size() const;
	int  at() const;	// not a pure virtual function
	bool at( int );		// not a pure virtual function
	bool atEnd() const;	// not a pure virtual function

	int readBlock( char *data, uint maxlen );
	int writeBlock( const char *data, uint len );
	int readLine( char *data, uint maxlen );

	int getch();
	int putch( int );
	int ungetch( int );
    };
  \endcode

  The three non-pure virtual functions can be ignored if your device
  is sequential (e.g. an RS-232 port).

  \sa QDataStream, QTextStream
*/


/*!
  Constructs an I/O device.
*/

QIODevice::QIODevice()
{
    ioMode = 0;					// initial mode
    ioSt = IO_Ok;
    ioIndex = 0;
}

/*!
  Destructs the I/O device.
*/

QIODevice::~QIODevice()
{
}


/*!
  \fn int QIODevice::flags() const
  Returns the current I/O device flags setting.

  Flags consists of mode flags and state flags.

  \sa mode(), state()
*/

/*!
  \fn int QIODevice::mode() const
  Returns bits OR'ed together that specify the current operation mode.

  These are the flags that were given to the open() function.

  The flags are: \c IO_ReadOnly, \c IO_WriteOnly, \c IO_ReadWrite,
  \c IO_Append, \c IO_Truncate and \c IO_Translate.
*/

/*!
  \fn int QIODevice::state() const
  Returns bits OR'ed together that specify the current state.

  The flags are: \c IO_Open.

  Subclasses may define more flags.
*/

/*!
  \fn bool QIODevice::isDirectAccess() const
  Returns TRUE if the I/O device is a direct access (not sequential) device,
  otherwise FALSE.
  \sa isSequentialAccess()
*/

/*!
  \fn bool QIODevice::isSequentialAccess() const
  Returns TRUE if the I/O device is a sequential access (not direct) device,
  otherwise FALSE.  Operations involving size() and at(int) are not valid
  on sequential devices.
  \sa isDirectAccess()
*/

/*!
  \fn bool QIODevice::isCombinedAccess() const
  Returns TRUE if the I/O device is a combined access (both direct and
  sequential) device,  otherwise FALSE.

  This access method is currently not in use.
*/

/*!
  \fn bool QIODevice::isBuffered() const
  Returns TRUE if the I/O device is a buffered (not raw) device, otherwise
  FALSE.
  \sa isRaw()
*/

/*!
  \fn bool QIODevice::isRaw() const
  Returns TRUE if the I/O device is a raw (not buffered) device, otherwise
  FALSE.
  \sa isBuffered()
*/

/*!
  \fn bool QIODevice::isSynchronous() const
  Returns TRUE if the I/O device is a synchronous device, otherwise
  FALSE.
  \sa isAsynchronous()
*/

/*!
  \fn bool QIODevice::isAsynchronous() const
  Returns TRUE if the I/O device is a asynchronous device, otherwise
  FALSE.

  This mode is currently not in use.

  \sa isSynchronous()
*/

/*!
  \fn bool QIODevice::isTranslated() const
  Returns TRUE if the I/O device translates carriage-return and linefeed
  characters.

  A QFile is translated if it is opened with the \c IO_Translate mode
  flag.
*/

/*!
  \fn bool QIODevice::isReadable() const
  Returns TRUE if the I/O device was opened using \c IO_ReadOnly or
  \c IO_ReadWrite mode.
  \sa isWritable(), isReadWrite()
*/

/*!
  \fn bool QIODevice::isWritable() const
  Returns TRUE if the I/O device was opened using \c IO_WriteOnly or
  \c IO_ReadWrite mode.
  \sa isReadable(), isReadWrite()
*/

/*!
  \fn bool QIODevice::isReadWrite() const
  Returns TRUE if the I/O device was opened using \c IO_ReadWrite mode.
  \sa isReadable(), isWritable()
*/

/*!
  \fn bool QIODevice::isInactive() const
  Returns TRUE if the I/O device state is 0, i.e. the device is not open.
  \sa isOpen()
*/

/*!
  \fn bool QIODevice::isOpen() const
  Returns TRUE if the I/O device state has been opened, otherwise FALSE.
  \sa isInactive()
*/


/*!
  \fn int QIODevice::status() const
  Returns the I/O device status.

  The I/O device status returns an error code.	If open() returns FALSE
  or readBlock() or writeBlock() return -1, this function can be called to
  get the reason why the operation did not succeed.

  The status codes are:
  <ul>
  <li>\c IO_Ok The operation was successful.
  <li>\c IO_ReadError Could not read from the device.
  <li>\c IO_WriteError Could not write to the device.
  <li>\c IO_FatalError A fatal unrecoverable error occurred.
  <li>\c IO_OpenError Could not open the device.
  <li>\c IO_ConnectError Could not connect to the device.
  <li>\c IO_AbortError The operation was unexpectedly aborted.
  <li>\c IO_TimeOutError The operation timed out.
  <li>\c IO_OnCloseError An unspecified error happened on close.
  </ul>

  \sa resetStatus()
*/

/*!
  \fn void QIODevice::resetStatus()

  Sets the I/O device status to \c IO_Ok.

  \sa status()
*/


/*!
  \fn void QIODevice::setFlags( int f )
  \internal
  Used by subclasses to set the device flags.
*/

/*!
  \internal
  Used by subclasses to set the device type.
*/

void QIODevice::setType( int t )
{
#if defined(CHECK_RANGE)
    if ( (t & IO_TypeMask) != t )
	qWarning( "QIODevice::setType: Specified type out of range" );
#endif
    ioMode &= ~IO_TypeMask;			// reset type bits
    ioMode |= t;
}

/*!
  \internal
  Used by subclasses to set the device mode.
*/

void QIODevice::setMode( int m )
{
#if defined(CHECK_RANGE)
    if ( (m & IO_ModeMask) != m )
	qWarning( "QIODevice::setMode: Specified mode out of range" );
#endif
    ioMode &= ~IO_ModeMask;			// reset mode bits
    ioMode |= m;
}

/*!
  \internal
  Used by subclasses to set the device state.
*/

void QIODevice::setState( int s )
{
#if defined(CHECK_RANGE)
    if ( ((uint)s & IO_StateMask) != (uint)s )
	qWarning( "QIODevice::setState: Specified state out of range" );
#endif
    ioMode &= ~IO_StateMask;			// reset state bits
    ioMode |= (uint)s;
}

/*!
  \internal
  Used by subclasses to set the device status (not state).
*/

void QIODevice::setStatus( int s )
{
    ioSt = s;
}


/*!
  \fn bool QIODevice::open( int mode )
  Opens the I/O device using the specified \e mode.
  Returns TRUE if successful, or FALSE if the device could not be opened.

  The mode parameter \e m must be a combination of the following flags.
  <ul>
  <li>\c IO_Raw specified raw (unbuffered) file access.
  <li>\c IO_ReadOnly opens a file in read-only mode.
  <li>\c IO_WriteOnly opens a file in write-only mode.
  <li>\c IO_ReadWrite opens a file in read/write mode.
  <li>\c IO_Append sets the file index to the end of the file.
  <li>\c IO_Truncate truncates the file.
  <li>\c IO_Translate enables carriage returns and linefeed translation
  for text files under MS-DOS, Window, OS/2 and Macintosh.  On Unix systems 
  this flag has no effect. Use with caution as it will also transform every linefeed
  written to the file into a CRLF pair. This is likely to corrupt your file when
  writing binary data to it. Cannot be combined with \c IO_Raw.
  </ul>

  This virtual function must be reimplemented by all subclasses.

  \sa close()
*/

/*!
  \fn void QIODevice::close()
  Closes the I/O device.

  This virtual function must be reimplemented by all subclasses.

  \sa open()
*/

/*!
  \fn void QIODevice::flush()

  Flushes an open I/O device.

  This virtual function must be reimplemented by all subclasses.
*/


/*!
  \fn uint QIODevice::size() const
  Virtual function that returns the size of the I/O device.
  \sa at()
*/

/*!
  Virtual function that returns the current I/O device index.

  This index is the data read/write head of the I/O device.

  \sa size()
*/

int QIODevice::at() const
{
    return ioIndex;
}

/*!
  Virtual function that sets the I/O device index to \e pos.
  \sa size()
*/

bool QIODevice::at( int pos )
{
#if defined(CHECK_RANGE)
    if ( (uint)pos > size() ) {
	qWarning( "QIODevice::at: Index %d out of range", pos );
	return FALSE;
    }
#endif
    ioIndex = pos;
    return TRUE;
}

/*!
  Virtual function that returns TRUE if the I/O device index is at the
  end of the input.
*/

bool QIODevice::atEnd() const
{
    if ( isSequentialAccess() || isTranslated() ) {
	QIODevice* that = (QIODevice*)this;
	int c = that->getch();
	bool result = c < 0;
	that->ungetch(c);
	return result;
    } else {
	return at() == (int)size();
    }
}

/*!
  \fn bool QIODevice::reset()
  Sets the device index to 0.
  \sa at()
*/


/*!
  \fn int QIODevice::readBlock( char *data, uint maxlen )
  Reads at most \e maxlen bytes from the I/O device into \e data and
  returns the number of bytes actually read.

  This virtual function must be reimplemented by all subclasses.

  \sa writeBlock()
*/

/*!
  This convenience function returns all of the remaining data in the
  device.  Note that this only works for direct access devices, such
  as QFile.
  
  \sa isDirectAccess() 
*/
QByteArray QIODevice::readAll()
{
    int n = size()-at();
    QByteArray ba(size()-at());
    char* c = ba.data();
    while ( n ) {
	int r = readBlock( c, n );
	if ( r < 0 )
	    return QByteArray();
	n -= r;
	c += r;
    }
    return ba;
}

/*!
  \fn int QIODevice::writeBlock( const char *data, uint len )
  Writes \e len bytes from \e p to the I/O device and returns the number of
  bytes actually written.

  This virtual function must be reimplemented by all subclasses.

  \sa readBlock()
*/

/*!
  This convenience function is the same as calling
  writeBlock( data.data(), data.size() ).
*/
int QIODevice::writeBlock( const QByteArray& data )
{
    return writeBlock( data.data(), data.size() );
}

/*!
  Reads a line of text, up to \e maxlen bytes including a terminating
  \0.  If there is a newline at the end if the line, it is not stripped.

  Returns the number of bytes read, or -1 in case of error.

  This virtual function can be reimplemented much more efficiently by
  the most subclasses.

  \sa readBlock(), QTextStream::readLine()
*/

int QIODevice::readLine( char *data, uint maxlen )
{
    if ( maxlen == 0 )				// application bug?
	return 0;
    int pos = at();				// get current position
    int s  = (int)size();			// size of I/O device
    char *p = data;
    if ( pos >= s )
	return 0;
    while ( pos++ < s && --maxlen ) {		// read one byte at a time
	readBlock( p, 1 );
	if ( *p++ == '\n' )			// end of line
	    break;
    }
    *p++ = '\0';
    return (int)((long)p - (long)data);
}


/*!
  \fn int QIODevice::getch()

  Reads a single byte/character from the I/O device.

  Returns the byte/character read, or -1 if the end of the I/O device has been
  reached.

  This virtual function must be reimplemented by all subclasses.

  \sa putch(), ungetch()
*/

/*!
  \fn int QIODevice::putch( int ch )

  Writes the character \e ch to the I/O device.

  Returns \e ch, or -1 if some error occurred.

  This virtual function must be reimplemented by all subclasses.

  \sa getch(), ungetch()
*/

/*!
  \fn int QIODevice::ungetch( int ch )

  Puts the character \e ch back into the I/O device and decrements the
  index if it is not zero.

  This function is normally called to "undo" a getch() operation.

  Returns \e ch, or -1 if some error occurred.

  This virtual function must be reimplemented by all subclasses.

  \sa getch(), putch()
*/
