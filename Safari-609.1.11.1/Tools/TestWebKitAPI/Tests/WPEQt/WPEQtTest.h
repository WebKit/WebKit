/*
 * Copyright (C) 2018, 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

// WPEQt has to be included before the remaining Qt headers, because of epoxy.
#include <wpe/qt/WPEQtView.h>
#include <wpe/qt/WPEQtViewLoadRequest.h>

#include <QEventLoop>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

class LoadSpy : public QEventLoop {
    Q_OBJECT

public:
    LoadSpy(WPEQtView *webView)
    {
        connect(webView, SIGNAL(loadingChanged(WPEQtViewLoadRequest*)), SLOT(onLoadingChanged(WPEQtViewLoadRequest*)));
    }

    ~LoadSpy() { }

Q_SIGNALS:
    void loadSucceeded();
    void loadFailed();

private Q_SLOTS:
    void onLoadingChanged(WPEQtViewLoadRequest *loadRequest)
    {
        if (loadRequest->status() == WPEQtView::LoadSucceededStatus)
            Q_EMIT loadSucceeded();
        else if (loadRequest->status() == WPEQtView::LoadFailedStatus)
            Q_EMIT loadFailed();
    }
};

class LoadStartedCatcher : public QObject {
    Q_OBJECT

public:
    LoadStartedCatcher(WPEQtView *webView)
        : m_webView(webView)
    {
        connect(m_webView, SIGNAL(loadingChanged(WPEQtViewLoadRequest*)), this, SLOT(onLoadingChanged(WPEQtViewLoadRequest*)));
    }

    virtual ~LoadStartedCatcher() { }

public Q_SLOTS:
    void onLoadingChanged(WPEQtViewLoadRequest *loadRequest)
    {
        if (loadRequest->status() == WPEQtView::LoadStartedStatus)
            QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }

Q_SIGNALS:
    void finished();

private:
    WPEQtView* m_webView;
};

bool waitForSignal(QObject*, const char* signal, int timeout = 10000);

inline bool waitForLoadSucceeded(WPEQtView* webView, int timeout = 10000)
{
    LoadSpy loadSpy(webView);
    return waitForSignal(&loadSpy, SIGNAL(loadSucceeded()), timeout);
}

inline bool waitForLoadFailed(WPEQtView* webView, int timeout = 10000)
{
    LoadSpy loadSpy(webView);
    return waitForSignal(&loadSpy, SIGNAL(loadFailed()), timeout);
}

class WPEQtTest: public QObject {
    Q_OBJECT
public:
    WPEQtTest()
        : m_argc(0)
        , m_app(m_argc, nullptr)
        , m_engine()
    {
        g_setenv("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH, FALSE);
        g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);

        QQmlComponent component(&m_engine);
        component.setData("import QtQuick 2.11\n"
            "import QtQuick.Window 2.11\n"
            "import org.wpewebkit.qtwpe 1.0\n"
            "Window {\n"
            "    id: main_window\n"
            "    visible: true\n"
            "    width: 1280\n"
            "    height: 720\n"
            "    WPEView {\n"
            "        objectName: \"wpeview\"\n"
            "        focus: true\n"
            "        anchors.fill: parent\n"
            "    }\n"
            "}", QUrl());

        QObject* object = component.create();
        m_view = object->findChild<WPEQtView*>("wpeview");

        QTRY_COMPARE(m_view->loadProgress(), 0);
    }

    virtual ~WPEQtTest()
    {
        delete m_view;
    }

private Q_SLOTS:
    void run()
    {
        QObject::connect(m_view, &WPEQtView::webViewCreated, this, [this] {
            main();
            m_app.quit();
        });
        m_app.exec();
    }

private:
    virtual void main() = 0;

    int m_argc;
    QGuiApplication m_app;

protected:
    WPEQtView* m_view { nullptr };
    QQmlEngine m_engine;
};
