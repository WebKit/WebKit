/****************************************************************************
** $Id$
**
** Definition of QStrList, QStrIList and QStrListIterator classes
**
** Created : 920730
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

#ifndef QSTRLIST_H
#define QSTRLIST_H

// KWQ hacks ---------------------------------------------------------------

#ifndef USING_BORROWED_QSTRINGLIST
#define USING_BORROWED_QSTRINGLIST
#endif

#include <KWQDef.h>

#ifndef USING_BORROWED_QSTRING
#include <string.h>
#define qstrcmp(s1,s2) strcmp((s1),(s2))
#define qstricmp(s1,s2) strcasecmp((s1),(s2))
#define qstrdup(s) strdup((s))
#endif

// -------------------------------------------------------------------------

#ifndef QT_H
#include "qstring.h"
#include "qlist.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif

#endif // QT_H


#if defined(Q_TEMPLATEDLL)
template class Q_EXPORT QList<char>;
template class Q_EXPORT QListIterator<char>;
#endif

typedef QList<char>		QStrListBase;
typedef QListIterator<char>	QStrListIterator;


class Q_EXPORT QStrList : public QStrListBase
{
public:
    QStrList( bool deepCopies=TRUE ) { dc = deepCopies; del_item = deepCopies; }
    QStrList( const QStrList & );
   ~QStrList()			{ clear(); }
    QStrList& operator=( const QStrList & );

private:
    QCollection::Item newItem( QCollection::Item d ) { return dc ? qstrdup( (const char*)d ) : d; }
    void deleteItem( QCollection::Item d ) { if ( del_item ) delete[] (char*)d; }
    int compareItems( QCollection::Item s1, QCollection::Item s2 ) { return qstrcmp((const char*)s1,
							 (const char*)s2); }
#ifndef QT_NO_DATASTREAM
    QDataStream &read( QDataStream &s, QCollection::Item &d )
				{ s >> (char *&)d; return s; }
    QDataStream &write( QDataStream &s, QCollection::Item d ) const
				{ return s << (const char *)d; }
#endif
    bool  dc;
};


class Q_EXPORT QStrIList : public QStrList	// case insensitive string list
{
public:
    QStrIList( bool deepCopies=TRUE ) : QStrList( deepCopies ) {}
   ~QStrIList()			{ clear(); }
private:
    int	  compareItems( QCollection::Item s1, QCollection::Item s2 )
				{ return qstricmp((const char*)s1,
						    (const char*)s2); }
};


inline QStrList & QStrList::operator=( const QStrList &strList )
{
    clear();
    dc = strList.dc;
    del_item = dc;
    QStrListBase::operator=(strList);
    return *this;
}

inline QStrList::QStrList( const QStrList &strList )
    : QStrListBase( strList )
{
    dc = FALSE;
    operator=(strList);
}


#endif // QSTRLIST_H
