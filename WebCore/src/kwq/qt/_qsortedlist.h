/****************************************************************************
** $Id$
**
** Definition of QList template/macro class
**
** Created : 920701
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

#ifndef QSORTEDLIST_H
#define QSORTEDLIST_H

// KWQ hacks ---------------------------------------------------------------

#ifndef USING_BORROWED_QSORTEDLIST
#define USING_BORROWED_QSORTEDLIST
#endif

#include <KWQDef.h>

// -------------------------------------------------------------------------

#ifndef QT_H
#include "qlist.h"
#endif // QT_H


template<class type> class Q_EXPORT QSortedList : public QList<type>
{
public:
    QSortedList() {}
    QSortedList( const QSortedList<type> &l ) : QList<type>(l) {}
    ~QSortedList() { clear(); }
    QSortedList<type> &operator=(const QSortedList<type> &l)
      { return (QSortedList<type>&)QList<type>::operator=(l); }

    virtual int compareItems( QCollection::Item s1, QCollection::Item s2 )
      { if ( *((type*)s1) == *((type*)s2) ) return 0; return ( *((type*)s1) < *((type*)s2) ? -1 : 1 ); }
};

#endif
