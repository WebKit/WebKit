/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RunLoop.h"
#include <runtime/InitializeThreading.h>
#include "WebProcess.h"
#include <wtf/Threading.h>

#include <QApplication>
#include <QList>
#include <QNetworkProxyFactory>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtGlobal>

#if USE(MEEGOTOUCH)
#include <MComponentData>
#endif

#ifndef NDEBUG
#if !OS(WINDOWS)
#include <unistd.h>
#endif
#endif

#ifndef NDEBUG
#include <QDebug>
#endif

using namespace WebCore;

namespace WebKit {
#ifndef NDEBUG
#if OS(WINDOWS)
static void sleep(unsigned seconds)
{
    ::Sleep(seconds * 1000);
}
#endif
#endif

class EnvHttpProxyFactory : public QNetworkProxyFactory
{
public:
    EnvHttpProxyFactory() { }

    bool initializeFromEnvironment();

    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery& query = QNetworkProxyQuery());

private:
    QList<QNetworkProxy> m_httpProxy;
    QList<QNetworkProxy> m_httpsProxy;
};

bool EnvHttpProxyFactory::initializeFromEnvironment()
{
    bool wasSetByEnvironment = false;

    QUrl proxyUrl = QUrl::fromUserInput(QString::fromLocal8Bit(qgetenv("http_proxy")));
    if (proxyUrl.isValid() && !proxyUrl.host().isEmpty()) {
        int proxyPort = (proxyUrl.port() > 0) ? proxyUrl.port() : 8080;
        m_httpProxy << QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyPort);
        wasSetByEnvironment = true;
    } else
        m_httpProxy << QNetworkProxy::NoProxy;

    proxyUrl = QUrl::fromUserInput(QString::fromLocal8Bit(qgetenv("https_proxy")));
    if (proxyUrl.isValid() && !proxyUrl.host().isEmpty()) {
        int proxyPort = (proxyUrl.port() > 0) ? proxyUrl.port() : 8080;
        m_httpsProxy << QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyPort);
        wasSetByEnvironment = true;
    } else
        m_httpsProxy << QNetworkProxy::NoProxy;

    return wasSetByEnvironment;
}

QList<QNetworkProxy> EnvHttpProxyFactory::queryProxy(const QNetworkProxyQuery& query)
{
    QString protocol = query.protocolTag().toLower();
    if (protocol == QLatin1String("http"))
        return m_httpProxy;
    else if (protocol == QLatin1String("https"))
        return m_httpsProxy;

    QList<QNetworkProxy> proxies;
    proxies << QNetworkProxy::NoProxy;
    return proxies;
}

static void initializeProxy()
{
    QList<QNetworkProxy> proxylist = QNetworkProxyFactory::systemProxyForQuery();
    if (proxylist.count() == 1) {
        QNetworkProxy proxy = proxylist.first();
        if (proxy == QNetworkProxy::NoProxy || proxy == QNetworkProxy::DefaultProxy) {
            EnvHttpProxyFactory* proxyFactory = new EnvHttpProxyFactory();
            if (proxyFactory->initializeFromEnvironment()) {
                QNetworkProxyFactory::setApplicationProxyFactory(proxyFactory);
                return;
            }
        }
    }
    QNetworkProxyFactory::setUseSystemConfiguration(true);
}

Q_DECL_EXPORT int WebProcessMainQt(int argc, char** argv)
{
    QApplication::setGraphicsSystem(QLatin1String("raster"));
    QApplication* app = new QApplication(argc, argv);
#ifndef NDEBUG
    if (!qgetenv("WEBKIT2_PAUSE_WEB_PROCESS_ON_LAUNCH").isEmpty()) {
        qDebug() << "Waiting 3 seconds for debugger";
        sleep(3);
    }
#endif

#if USE(MEEGOTOUCH)
    new MComponentData(argc, argv);
#endif

    initializeProxy();

    srandom(time(0));

    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    // Create the connection.
    if (app->arguments().size() <= 1) {
        qDebug() << "Error: wrong number of arguments.";
        return 1;
    }

    bool wasNumber = false;
    int identifier = app->arguments().at(1).toInt(&wasNumber, 10);
    if (!wasNumber) {
        qDebug() << "Error: connection identifier wrong.";
        return 1;
    }

    WebKit::WebProcess::shared().initialize(identifier, RunLoop::main());

    RunLoop::run();

    // FIXME: Do more cleanup here.

    return 0;
}

}
