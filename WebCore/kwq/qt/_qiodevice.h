/****************************************************************************
** $Id$
**
** Definition of QIODevice class
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

#ifndef QIODEVICE_H
#define QIODEVICE_H

#ifndef QT_H
#include <KWQDef.h>
#include "qcstring.h"
#endif // QT_H


// IO device access types

#define IO_Direct		0x0100		// direct access device
#define IO_Sequential		0x0200		// sequential access device
#define IO_Combined		0x0300		// combined direct/sequential
#define IO_TypeMask		0x0f00

// IO handling modes

#define IO_Raw			0x0040		// raw access (not buffered)
#define IO_Async		0x0080		// asynchronous mode

// IO device open modes

#define IO_ReadOnly		0x0001		// readable device
#define IO_WriteOnly		0x0002		// writable device
#define IO_ReadWrite		0x0003		// read+write device
#define IO_Append		0x0004		// append
#define IO_Truncate		0x0008		// truncate device
#define IO_Translate		0x0010		// translate CR+LF
#define IO_ModeMask		0x00ff

// IO device state

#define IO_Open			0x1000		// device is open
#define IO_StateMask		0xf000


// IO device status

#define IO_Ok			0
#define IO_ReadError		1		// read error
#define IO_WriteError		2		// write error
#define IO_FatalError		3		// fatal unrecoverable error
#define IO_ResourceError	4		// resource limitation
#define IO_OpenError		5		// cannot open device
#define IO_ConnectError		5		// cannot connect to device
#define IO_AbortError		6		// abort error
#define IO_TimeOutError		7		// time out
#define IO_UnspecifiedError		8		// unspecified error

class Q_EXPORT QIODevice					// IO device class
{
public:
    QIODevice();
    virtual ~QIODevice();

    int		 flags()  const { return ioMode; }
    int		 mode()	  const { return ioMode & IO_ModeMask; }
    int		 state()  const { return ioMode & IO_StateMask; }

    bool	 isDirectAccess()     const { return ((ioMode & IO_Direct)     == IO_Direct); }
    bool	 isSequentialAccess() const { return ((ioMode & IO_Sequential) == IO_Sequential); }
    bool	 isCombinedAccess()   const { return ((ioMode & IO_Combined)   == IO_Combined); }
    bool	 isBuffered()	      const { return ((ioMode & IO_Raw)        != IO_Raw); }
    bool	 isRaw()	      const { return ((ioMode & IO_Raw)        == IO_Raw); }
    bool	 isSynchronous()      const { return ((ioMode & IO_Async)      != IO_Async); }
    bool	 isAsynchronous()     const { return ((ioMode & IO_Async)      == IO_Async); }
    bool	 isTranslated()	      const { return ((ioMode & IO_Translate)  == IO_Translate); }
    bool	 isReadable()	      const { return ((ioMode & IO_ReadOnly)   == IO_ReadOnly); }
    bool	 isWritable()	      const { return ((ioMode & IO_WriteOnly)  == IO_WriteOnly); }
    bool	 isReadWrite()	      const { return ((ioMode & IO_ReadWrite)  == IO_ReadWrite); }
    bool	 isInactive()	      const { return state() == 0; }
    bool	 isOpen()	      const { return state() == IO_Open; }

    int		 status() const { return ioSt; }
    void	 resetStatus()	{ ioSt = IO_Ok; }

    virtual bool open( int mode ) = 0;
    virtual void close() = 0;
    virtual void flush() = 0;

    virtual uint size()	  const = 0;
    virtual int	 at()	  const;
    virtual bool at( int );
    virtual bool atEnd()  const;
    bool	 reset() { return at(0); }

    virtual int	 readBlock( char *data, uint maxlen ) = 0;
    virtual int	 writeBlock( const char *data, uint len ) = 0;
    virtual int	 readLine( char *data, uint maxlen );
    int writeBlock( const QByteArray& data );
    QByteArray readAll();

    virtual int	 getch() = 0;
    virtual int	 putch( int ) = 0;
    virtual int	 ungetch( int ) = 0;

protected:
    void	 setFlags( int f ) { ioMode = f; }
    void	 setType( int );
    void	 setMode( int );
    void	 setState( int );
    void	 setStatus( int );
    int		 ioIndex;

private:
    int		 ioMode;
    int		 ioSt;

private:	// Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    QIODevice( const QIODevice & );
    QIODevice &operator=( const QIODevice & );
#endif
};


#endif // QIODEVICE_H
