/****************************************************************************
** $Id$
**
** Definition of QBuffer class
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

#ifndef QBUFFER_H
#define QBUFFER_H

#ifndef QT_H
#include "_qiodevice.h"
#include "qstring.h"
#endif // QT_H


class Q_EXPORT QBuffer : public QIODevice
{
public:
    QBuffer();
    QBuffer( QByteArray );
   ~QBuffer();

    QByteArray buffer() const;
    bool  setBuffer( QByteArray );

    bool  open( int );
    void  close();
    void  flush();

    uint  size() const;
    int	  at()	 const;
    bool  at( int );

    int	  readBlock( char *p, uint );
    int	  writeBlock( const char *p, uint );
    int	  readLine( char *p, uint );

    int	  getch();
    int	  putch( int );
    int	  ungetch( int );

protected:
    QByteArray a;

private:
    uint  a_len;
    uint  a_inc;

private:	// Disabled copy constructor and operator=
#if defined(Q_DISABLE_COPY)
    QBuffer( const QBuffer & );
    QBuffer &operator=( const QBuffer & );
#endif
};


inline QByteArray QBuffer::buffer() const
{ return a; }

inline uint QBuffer::size() const
{ return a.size(); }

inline int QBuffer::at() const
{ return ioIndex; }


#endif // QBUFFER_H
