/****************************************************************************
** $Id$
**
** Definition of QStringList class
**
** Created : 990406
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

#ifndef QSTRINGLIST_H
#define QSTRINGLIST_H

#ifndef QT_H
#include "qvaluelist.h"
#include "qstring.h"
#include "qregexp.h"
#endif // QT_H

#ifndef QT_NO_STRINGLIST

class QStrList;

class Q_EXPORT QStringList : public QValueList<QString>
{
public:
    QStringList() { }
    QStringList( const QStringList& l ) : QValueList<QString>(l) { }
    QStringList( const QValueList<QString>& l ) : QValueList<QString>(l) { }
    QStringList( const QString& i ) { append(i); }
#ifndef QT_NO_CAST_ASCII
    QStringList( const char* i ) { append(i); }
#endif

    static QStringList fromStrList(const QStrList&);

    void sort();

    static QStringList split( const QString &sep, const QString &str, bool allowEmptyEntries = FALSE );
    static QStringList split(  const QChar &sep, const QString &str, bool allowEmptyEntries = FALSE );
    static QStringList split(  const QRegExp &sep, const QString &str, bool allowEmptyEntries = FALSE );
    QString join( const QString &sep ) const;

    QStringList grep( const QString &str, bool cs = TRUE ) const;
    QStringList grep( const QRegExp &expr ) const;
};

#ifndef QT_NO_DATASTREAM
class QDataStream;
extern Q_EXPORT QDataStream &operator>>( QDataStream &, QStringList& );
extern Q_EXPORT QDataStream &operator<<( QDataStream &, const QStringList& );
#endif
#endif // QT_NO_STRINGLIST
#endif // QSTRINGLIST_H
