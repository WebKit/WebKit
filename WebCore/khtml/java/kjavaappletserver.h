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

#ifndef KJAVAAPPLETSERVER_H
#define KJAVAAPPLETSERVER_H

#include <kjavaprocess.h>
#include <qobject.h>
#include <qmap.h>


/**
 * @short Communicates with a KJAS server to display and control Java applets.
 *
 * @author Richard J. Moore, rich@kde.org
 */

class KJavaAppletContext;
class KJavaAppletServerPrivate;

class KJavaAppletServer : public QObject
{
Q_OBJECT

public:
    /**
     * Create the applet server.  These shouldn't be used directly,
     * use allocateJavaServer instead
     */
    KJavaAppletServer();
    ~KJavaAppletServer();

    /**
     * A factory method that returns the default server. This is the way this
     * class is usually instantiated.
     */
    static KJavaAppletServer *allocateJavaServer();

    /**
     * When you are done using your reference to the AppletServer,  you must
     * dereference it by calling freeJavaServer().
     */
    static void freeJavaServer();

    /**
     * This allows the KJavaAppletWidget to display some feedback in a QLabel
     * while the applet is being loaded.  If the java process could not be
     * started, an error message is displayed instead.
     */
    static QString getAppletLabel();

    /**
     * Create an applet context with the specified id.
     */
    void createContext( int contextId, KJavaAppletContext* context );

    /**
     * Destroy the applet context with the specified id. All the applets in the
     * context will be destroyed as well.
     */
    void destroyContext( int contextId );

    /**
     * Create an applet in the specified context with the specified id. The applet
     * name, class etc. are specified in the same way as in the HTML APPLET tag.
     */
    void createApplet( int contextId, int appletId,
                       const QString name, const QString clazzName,
                       const QString baseURL, const QString codeBase,
                       const QString jarFile, QSize size,
                       const QMap<QString, QString>& params,
                       const QString windowTitle );

    /**
     * This should be called by the KJavaAppletWidget
     */
    void initApplet( int contextId, int appletId );

    /**
     * Destroy an applet in the specified context with the specified id.
     */
    void destroyApplet( int contextId, int appletId );

    /**
     * Start the specified applet.
     */
    void startApplet( int contextId, int appletId );

    /**
     * Stop the specified applet.
     */
    void stopApplet( int contextId, int appletId );

    /**
     * Send data we got back from a KJavaDownloader back to the appropriate
     * class loader.
     * (This is currently unimplemented on the java side.
     */
    void sendURLData( const QString& loaderID, const QString& url,
                      const QByteArray& data );

    /**
     * Shut down the KJAS server.
     */
    void quit();

    QString appletLabel();

protected:
    void setupJava( KJavaProcess* p );

    KJavaProcess* process;

protected slots:
    void slotJavaRequest( const QByteArray& qb );
    void checkShutdown();

private:
    KJavaAppletServerPrivate* d;

};

#endif // KJAVAAPPLETSERVER_H
