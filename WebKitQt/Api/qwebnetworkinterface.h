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
#ifndef QWEBNETWORKINTERFACE_H
#define QWEBNETWORKINTERFACE_H

#include <qobject.h>
#include <qurl.h>
#include <qhttp.h>
#include <qbytearray.h>

#include <qwebkitglobal.h>

class QWebNetworkJobPrivate;
class QWebNetworkInterface;

namespace WebCore {
    class WebCoreHttp;
}

class QWEBKIT_EXPORT QWebNetworkJob
{
public:
    QUrl url() const;
    QByteArray postData() const;
    QHttpRequestHeader request() const;

    QHttpResponseHeader response() const;
    void setResponse(const QHttpResponseHeader &response);

    bool cancelled() const;

    void ref();
    bool deref();

    QWebNetworkInterface *networkInterface() const;
    
private:
    QWebNetworkJob();
    ~QWebNetworkJob();

    friend class QWebNetworkManager;
    QWebNetworkJobPrivate *d;
};

class QWebNetworkInterfacePrivate;

class QWEBKIT_EXPORT QWebNetworkInterface : public QObject
{
    Q_OBJECT
public:
    QWebNetworkInterface(QObject *parent = 0);
    ~QWebNetworkInterface();

    static void setDefaultInterface(QWebNetworkInterface *defaultInterface);
    static QWebNetworkInterface *defaultInterface();

    virtual void addJob(QWebNetworkJob *job);
    virtual void cancelJob(QWebNetworkJob *job);
    
signals:
    void started(QWebNetworkJob*);
    void data(QWebNetworkJob*, const QByteArray &data);
    void finished(QWebNetworkJob*, int errorCode);

private:
    Q_PRIVATE_SLOT(d, void httpConnectionClosed(const WebCore::HostInfo &));
    friend class QWebNetworkInterfacePrivate;
    friend class WebCore::WebCoreHttp;
    QWebNetworkInterfacePrivate *d;
};

#endif
