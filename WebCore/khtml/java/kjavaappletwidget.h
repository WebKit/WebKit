// -*- c++ -*-

/* This file is part of the KDE project
 *
 * Copyright (C) 2000 Richard Moore <rich@kde.org>
 *               2000 Wynn Wilkes <wynnw@caldera.com>
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

#ifndef KJAVAAPPLETWIDGET_H
#define KJAVAAPPLETWIDGET_H

#include "kjavaappletcontext.h"
#include "kjavaapplet.h"
#include <javaembed.h>
#include <kwinmodule.h>

/**
 * @short A widget for displaying Java applets
 *
 * KJavaAppletWidget provides support for the inclusion of Java applets
 * in Qt and KDE applications. To create an applet, you must first create
 * a context object in which it will run. There can be several applets and
 * contexts in operation at a given time, for example in a web browser there
 * would be one context object for each web page. Applets in the same context
 * can communicate with each other, applets in different contexts cannot.
 * Once you have created a KJavaAppletContext, you can create as many
 * applets in it as you want.
 *
 * Once you have created the applet widget, you should access the applet() method
 * to call the various setXXX methods to configure the applet,
 * They correspond to the HTML tags used to embed applets in a web page.
 * Once the applet is configured, call the create() method to set things in motion.
 * The applet is running when it first appears, but you can start or stop it
 * when you like (for example if it scrolls off the screen).
 *
 * This widget works by using the KJavaAppletServer, which fires off a
 * Java server process with which it communicates using the
 * KDE Java Applet Server (KJAS) protocol over stdin and stdout.
 * The applet windows are swallowed and attached to the QWidget, but they are
 * actually running in a different process. This has the advantage of robustness
 * and reusability. The details of the communication are hidden from the user
 * in the KJASAppletServer class. Normally only a single server process is used for
 * all of the applets in a given application, this is all sorted automatically.
 * The KJAS server is 100% pure Java, and should also prove useful for people
 * wishing to add java support to other systems (for example a perl/Tk binding
 * is perfectly feasible). All you need to do is implement the protocol and
 * (optionally) swallow the applet windows.
 *
 * The applet support in KDE is still dependent on the KWin Window Manager.  The
 * applet swallowing will not work under other Window Managers.  Hopefully this
 * will be fixed in the future.
 *
 * For a description of the KJAS protocol, please see the KJAS_GRAMMAR.txt file.
 *
 * @author Richard J. Moore, rich@kde.org
 * @author Wynn Wilkes, wynnw@caldera.com
 */

class KJavaAppletWidgetPrivate;

class KJavaAppletWidget : public KJavaEmbed
{
    Q_OBJECT
public:
    KJavaAppletWidget( KJavaAppletContext* context,
                       QWidget* parent=0, const char* name=0 );

   ~KJavaAppletWidget();

    /**
     * Returns a pointer to the KJavaApplet.  Use this to
     * configure the applet's parameters. You can also
     * use it to start and stop the Applet.
     */
    KJavaApplet* applet() { return m_applet; }

    /**
     * Tells the AppletServer to create, initialize, and
     * show the Applet.
     */
    void showApplet();

    QSize sizeHint();
    void resize( int, int );

protected slots:
    /**
     * This slot is called by KWin when new windows are added.  We check
     * to see if the window has the title we set.  If so we embed it.
     */
    void setWindow( WId w );

protected:
    //The counter to generate ID's for the applets
    static int appletCount;

private:
    KJavaAppletWidgetPrivate* d;

    KJavaApplet* m_applet;
    KWinModule*  m_kwm;
    QString      m_swallowTitle;

};

#endif // KJAVAAPPLETWIDGET_H

