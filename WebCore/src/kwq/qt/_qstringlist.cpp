/****************************************************************************
** $Id$
**
** Implementation of QStringList
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

// KWQ hacks ---------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USING_BORROWED_QSTRINGLIST

// -------------------------------------------------------------------------

#include "qstringlist.h"

#ifndef QT_NO_STRINGLIST
#include "_qstrlist.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif

#include "_qtl.h"

// NOT REVISED
/*!
  \class QStringList qstringlist.h
  \brief A list of strings.

  \ingroup qtl
  \ingroup tools
  \ingroup shared

  QStringList is basically a QValueList of QString objects. As opposed
  to QStrList, that stores pointers to characters, QStringList deals
  with real QString objects.  It is the class of choice whenever you
  work with unicode strings.

  Like QString itself, QStringList objects are implicit shared.
  Passing them around as value-parameters is both fast and safe.

  Example:
  \code
	QStringList list;

	// three different ways of appending values:
	list.append( "Torben");
	list += "Warwick";
	list << "Matthias" << "Arnt" << "Paul";

	// sort the list, Arnt's now first
	list.sort();

	// print it out
	for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it ) {
	    printf( "%s \n", (*it).latin1() );
	}
  \endcode

  Convenience methods such as sort(), split(), join() and grep() make
  working with QStringList easy.
*/

/*!
 \fn QStringList::QStringList()
  Creates an empty list.
*/

/*! \fn QStringList::QStringList( const QStringList& l )
  Creates a copy of the list. This function is very fast since
  QStringList is implicit shared. However, for the programmer this
  is the same as a deep copy. If this list or the original one or some
  other list referencing the same shared data is modified, then the
  modifying list makes a copy first.
*/

/*!
  \fn QStringList::QStringList (const QString & i)
  Constructs a string list consisting of the single string \a i.
  To make longer lists easily, use:
  \code
    QString s1,s2,s3;
    ...
    QStringList mylist = QStringList() << s1 << s2 << s3;
  \endcode
*/

/*!
  \fn QStringList::QStringList (const char* i)
  Constructs a string list consisting of the single latin-1 string \a i.
*/

/*! \fn QStringList::QStringList( const QValueList<QString>& l )

  Constructs a new string list that is a copy of \a l.
*/

/*!
  Sorts the list of strings in ascending order.

  Sorting is very fast. It uses the Qt Template Library's
  efficient HeapSort implementation that operates in O(n*log n).
*/
void QStringList::sort()
{
    qHeapSort(*this);
}

/*!
  Splits the string \a str using \a sep as separator. Returns the
  list of strings. If \a allowEmptyEntries is TRUE, also empty
  entries are inserted into the list, else not. So if you have
  a string 'abc..d.e.', a list which contains 'abc', 'd', and 'e'
  would be returned if \a allowEmptyEntries is FALSE, but
  a list containing 'abc', '', 'd', 'e' and '' would be returned if
  \a allowEmptyEntries is TRUE.
  If \a str doesn't contain \a sep, a stringlist
  with one item, which is the same as \a str, is returned.

  \sa join()
*/

QStringList QStringList::split( const QChar &sep, const QString &str, bool allowEmptyEntries )
{
    return split( QString( sep ), str, allowEmptyEntries );
}

/*!
  Splits the string \a str using \a sep as separator. Returns the
  list of strings. If \a allowEmptyEntries is TRUE, also empty
  entries are inserted into the list, else not. So if you have
  a string 'abc..d.e.', a list which contains 'abc', 'd', and 'e'
  would be returned if \a allowEmptyEntries is FALSE, but
  a list containing 'abc', '', 'd', 'e' and '' would be returned if
  \a allowEmptyEntries is TRUE.
  If \a str doesn't contain \a sep, a stringlist
  with one item, which is the same as \a str, is returned.

  \sa join()
*/

QStringList QStringList::split( const QString &sep, const QString &str, bool allowEmptyEntries )
{
    QStringList lst;

    int j = 0;
    int i = str.find( sep, j );

    while ( i != -1 ) {
	if ( str.mid( j, i - j ).length() > 0 )
	    lst << str.mid( j, i - j );
	else if ( allowEmptyEntries )
	    lst << QString::null;
	j = i + sep.length();
	i = str.find( sep, j );
    }

    int l = str.length() - 1;
    if ( str.mid( j, l - j + 1 ).length() > 0 )
	lst << str.mid( j, l - j + 1 );
    else if ( allowEmptyEntries )
	lst << QString::null;

    return lst;
}

/*!
  Splits the string \a str using the regular expression \a sep as separator. Returns the
  list of strings. If \a allowEmptyEntries is TRUE, also empty
  entries are inserted into the list, else not. So if you have
  a string 'abc..d.e.', a list which contains 'abc', 'd', and 'e'
  would be returned if \a allowEmptyEntries is FALSE, but
  a list containing 'abc', '', 'd', 'e' and '' would be returned if
  \a allowEmptyEntries is TRUE.
  If \a str doesn't contain \a sep, a stringlist
  with one item, which is the same as \a str, is returned.

  \sa join()
*/

QStringList QStringList::split( const QRegExp &sep, const QString &str, bool allowEmptyEntries )
{
    QStringList lst;

    int j = 0;
    int len = 0;
    int i = sep.match( str, j, &len );

    while ( i != -1 ) {
	if ( str.mid( j, i - j ).length() > 0 )
	    lst << str.mid( j, i - j );
	else if ( allowEmptyEntries )
	    lst << QString::null;
	j = i + len;
	i = sep.match( str, j, &len );
    }

    int l = str.length() - 1;
    if ( str.mid( j, l - j + 1 ).length() > 0 )
	lst << str.mid( j, l - j + 1 );
    else if ( allowEmptyEntries )
	lst << QString::null;

    return lst;
}

/*!
  Returns a list of all strings containing the substring \a str.

  If \a cs is TRUE, the grep is done case sensitively, else not.
*/

QStringList QStringList::grep( const QString &str, bool cs ) const
{
    QStringList res;
    for ( QStringList::ConstIterator it = begin(); it != end(); ++it )
	if ( (*it).contains( str, cs ) )
	    res << *it;

    return res;
}

/*!
  Returns a list of all strings containing a substring that matches
  the regular expression \a expr.
*/

QStringList QStringList::grep( const QRegExp &expr ) const
{
    QStringList res;
#ifdef USING_BORROWED_QSTRING
    for ( QStringList::ConstIterator it = begin(); it != end(); ++it )
	if ( (*it).contains( expr ) )
	    res << *it;
#endif
    return res;
}

/*!
  Joins the stringlist into a single string with each element
  separated by \a sep.

  \sa split()
*/
QString QStringList::join( const QString &sep ) const
{
    QString res;
    bool alredy = FALSE;
    for ( QStringList::ConstIterator it = begin(); it != end(); ++it ) {
	if ( alredy )
	    res += sep;
	alredy = TRUE;
	res += *it;
    }

    return res;
}

#ifndef QT_NO_DATASTREAM
Q_EXPORT QDataStream &operator>>( QDataStream & s, QStringList& l )
{
    return s >> (QValueList<QString>&)l;
}

Q_EXPORT QDataStream &operator<<( QDataStream & s, const QStringList& l )
{
    return s << (const QValueList<QString>&)l;
}
#endif

/*!
  Converts from a QStrList (ASCII) to a QStringList (Unicode).
*/
QStringList QStringList::fromStrList(const QStrList& ascii)
{
    QStringList res;
    const char * s;
    for ( QStrListIterator it(ascii); (s=it.current()); ++it )
	res << s;
    return res;
}

#endif //QT_NO_STRINGLIST

// KWQ hacks ---------------------------------------------------------------

#endif USING_BORROWED_QSTRINGLIST

// -------------------------------------------------------------------------
