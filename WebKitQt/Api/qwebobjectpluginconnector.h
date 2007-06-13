/*
  Copyright (C) 2007 Trolltech ASA

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  This class provides all functionality needed for loading images, style sheets and html
  pages from the web. It has a memory cache for these objects.
*/
#ifndef QWEBOBJECTPLUGINCONNECTOR_H
#define QWEBOBJECTPLUGINCONNECTOR_H

#include <qwebkitglobal.h>
#include <qobject.h>
#include <qwebnetworkinterface.h>

class QWebFrame;
class QWebPage;
class QWidget;
class QUrl;
class QWebObjectPluginConnectorPrivate;
class QWebFactoryLoader;
class QWebNetworkManager;

class QWEBKIT_EXPORT QWebObjectPluginConnector : public QObject
{
    Q_OBJECT
public:
    QWebFrame *frame() const;
    QWidget *pluginParentWidget() const;

    enum Target {
        Plugin,
        New,
        Self,
        Parent,
        Top
    };
    QWebNetworkJob *requestUrl(const QWebNetworkRequest &request, Target target = Plugin);

signals:
    void started(QWebNetworkJob*);
    void data(QWebNetworkJob*, const QByteArray &data);
    void finished(QWebNetworkJob*, int errorCode);

private:
    friend class QWebFactoryLoader;
    friend class QWebNetworkManager;
    QWebObjectPluginConnector(QWebFrame *frame);

    QWebObjectPluginConnectorPrivate *d;
};

#endif
