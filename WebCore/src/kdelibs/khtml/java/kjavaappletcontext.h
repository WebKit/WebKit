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

#ifndef KJAVAAPPLETCONTEXT_H
#define KJAVAAPPLETCONTEXT_H

#include <qobject.h>

/**
 * @short Provides a context for KJavaAppletWidgets
 *
 * Applets run in a context- (see the Java documentation for more information
 * on contexts).  Currently, each document in KHTML creates one context, in
 * which multiple applets can run.
 *
 * @author Richard J. Moore, rich@kde.org
 * @author Wynn Wilkes, wynnw@caldera.com
 */


class KJavaAppletServer;
class KJavaApplet;
class KJavaAppletContextPrivate;

class KJavaAppletContext : public QObject
{
Q_OBJECT

public:
    KJavaAppletContext();
    ~KJavaAppletContext();

    /**
     * Returns the ID of this context.
     */
    int  contextId();

    /**
     * Sets the ID of this context.
     */
    void setContextId( int id );

    /**
     * Sends a message to create the applet.
     */
    void create( KJavaApplet* );

    /**
     * Sends a message to destroy the applet.
     */
    void destroy( KJavaApplet* );

    /**
     * Sends a message to initialize the applet.
     */
    void init( KJavaApplet* );

    /**
     * Sends a message to start the applet.
     */
    void start( KJavaApplet* );

    /**
     * Sends a message to stop the applet.
     */
    void stop( KJavaApplet* );

    /**
     * use this for applet call backs, the AppletServer
     * calls this directly.
     */
    void processCmd( QString cmd, QStringList args );

signals:
    /**
     * Signals the KHMTL Part to show this as the status message.
     */
    void showStatus  ( const QString& txt );

    /**
     * Signals the KHTML Part to show a url in a given target
     */
    void showDocument( const QString& url, const QString& target );

protected:
    //The counter to generate ID's for the contexts
    static int contextCount;

    // The applet server this context is attached to.
    KJavaAppletServer* server;

protected slots:
    void received( const QString& cmd, const QStringList& arg );

private:
    int id;
    KJavaAppletContextPrivate* d;

};

#endif // KJAVAAPPLETCONTEXT_H
