/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DUMPRENDERTREE_H
#define DUMPRENDERTREE_H

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTextStream>
#include <QSocketNotifier>

#ifndef QT_NO_SSL
#include <QSslError>
#endif

#include <qwebpage.h>

QT_BEGIN_NAMESPACE
class QUrl;
class QFile;
QT_END_NAMESPACE

class QWebFrame;

class LayoutTestController;
class EventSender;
class TextInputController;
class GCController;

namespace WebCore {

class WebPage;

class DumpRenderTree : public QObject {
Q_OBJECT

public:
    DumpRenderTree();
    virtual ~DumpRenderTree();

    // Initialize in multi-file mode, used by run-webkit-tests.
    void open();

    // Initialize in single-file mode.
    void open(const QUrl& url);

    void setTextOutputEnabled(bool enable) { m_enableTextOutput = enable; }
    bool isTextOutputEnabled() { return m_enableTextOutput; }

    void setDumpPixels(bool);

    void closeRemainingWindows();
    void resetToConsistentStateBeforeTesting();

    LayoutTestController *layoutTestController() const { return m_controller; }
    EventSender *eventSender() const { return m_eventSender; }
    TextInputController *textInputController() const { return m_textInputController; }

    QWebPage *createWindow();
    int windowCount() const;

    WebPage *webPage() const { return m_page; }


#if defined(Q_WS_X11)
    static void initializeFonts();
#endif

public Q_SLOTS:
    void initJSObjects();
    void readStdin(int);
    void dump();
    void titleChanged(const QString &s);
    void connectFrame(QWebFrame *frame);
    void dumpDatabaseQuota(QWebFrame* frame, const QString& dbName);
    void statusBarMessage(const QString& message);

Q_SIGNALS:
    void quit();

private:
    QString dumpFramesAsText(QWebFrame* frame);
    QString dumpBackForwardList();
    LayoutTestController *m_controller;

    bool m_dumpPixels;
    QString m_expectedHash;

    WebPage *m_page;

    EventSender *m_eventSender;
    TextInputController *m_textInputController;
    GCController* m_gcController;

    QFile *m_stdin;
    QSocketNotifier* m_notifier;

    QList<QWidget *> windows;
    bool m_enableTextOutput;
};

class NetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    NetworkAccessManager(QObject* parent);

private slots:
#ifndef QT_NO_SSL
    void sslErrorsEncountered(QNetworkReply*, const QList<QSslError>&);
#endif
};

class WebPage : public QWebPage {
    Q_OBJECT
public:
    WebPage(QWidget *parent, DumpRenderTree *drt);

    QWebPage *createWindow(QWebPage::WebWindowType);

    void javaScriptAlert(QWebFrame *frame, const QString& message);
    void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID);
    bool javaScriptConfirm(QWebFrame *frame, const QString& msg);
    bool javaScriptPrompt(QWebFrame *frame, const QString& msg, const QString& defaultValue, QString* result);

    void resetSettings();

    virtual bool supportsExtension(QWebPage::Extension extension) const;
    virtual bool extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output);

public slots:
    bool shouldInterruptJavaScript() { return false; }

protected:
    bool acceptNavigationRequest(QWebFrame* frame, const QNetworkRequest& request, NavigationType type);
    bool isTextOutputEnabled() { return m_drt->isTextOutputEnabled(); }

private slots:
    void setViewGeometry(const QRect &r)
    {
        QWidget *v = view();
        if (v)
            v->setGeometry(r);
    }
private:
    DumpRenderTree *m_drt;
};

}

#endif
