/* This file is part of the KDE project
 *
 * Copyright (C) 2000 Wynn Wilkes <wynnw@caldera.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/****************************************************************************
   This is a modified version of QXEmbed from Troll Tech.  It's been modified
   to work better with java applets.
*****************************************************************************/

#ifndef KJAVAEMBED_H
#define KJAVAEMBED_H

#include <qwidget.h>

class KJavaEmbedPrivate;
class KJavaEmbed : public QWidget
{
    Q_OBJECT

public:

    KJavaEmbed( QWidget *parent=0, const char *name=0, WFlags f = 0 );
    ~KJavaEmbed();

    /**
     * Swallows the window with the given Window ID.
     */
    void embed( WId w );

    /**
     * Returns whether or not we have embedded our window
     */
    bool embedded() { if( window != 0 ) return true; else return false; }

    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    QSizePolicy sizePolicy() const;

    bool eventFilter( QObject *, QEvent * );

protected:
    bool event( QEvent * );

    void focusInEvent( QFocusEvent * );
    void focusOutEvent( QFocusEvent * );
    void resizeEvent(QResizeEvent *);

    //void showEvent( QShowEvent * );

    bool x11Event( XEvent* );

    bool focusNextPrevChild( bool next );

private:
    WId window;
    KJavaEmbedPrivate* d;
};


#endif
