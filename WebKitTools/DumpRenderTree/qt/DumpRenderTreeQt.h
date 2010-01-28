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

#ifndef DumpRenderTreeQt_h
#define DumpRenderTreeQt_h

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTextStream>
#include <QSocketNotifier>

#ifndef QT_NO_OPENSSL
#include <QSslError>
#endif

#include <qwebframe.h>
#include <qwebinspector.h>
#include <qwebpage.h>
#include <qwebview.h>

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

    // Initialize in single-file mode.
    void open(const QUrl& url);

    void setTextOutputEnabled(bool enable) { m_enableTextOutput = enable; }
    bool isTextOutputEnabled() { return m_enableTextOutput; }

    void setSingleFileMode(bool flag) { m_singleFileMode = flag; }
    bool isSingleFileMode() { return m_singleFileMode; }

    void setDumpPixels(bool);

    void closeRemainingWindows();
    void resetToConsistentStateBeforeTesting();

    LayoutTestController *layoutTestController() const { return m_controller; }
    EventSender *eventSender() const { return m_eventSender; }
    TextInputController *textInputController() const { return m_textInputController; }

    QWebPage *createWindow();
    int windowCount() const;

    void switchFocus(bool focused);

    WebPage *webPage() const { return m_page; }


#if defined(Q_WS_X11)
    static void initializeFonts();
#endif

public Q_SLOTS:
    void initJSObjects();

    void readLine();
    void processLine(const QString&);

    void dump();
    void titleChanged(const QString &s);
    void connectFrame(QWebFrame *frame);
    void dumpDatabaseQuota(QWebFrame* frame, const QString& dbName);
    void statusBarMessage(const QString& message);
    void windowCloseRequested();

Q_SIGNALS:
    void quit();
    void ready();

private Q_SLOTS:
    void showPage();
    void hidePage();

private:
    QString dumpFramesAsText(QWebFrame* frame);
    QString dumpBackForwardList();
    LayoutTestController *m_controller;

    bool m_dumpPixels;
    QString m_expectedHash;

    WebPage *m_page;
    QWebView* m_mainView;

    EventSender *m_eventSender;
    TextInputController *m_textInputController;
    GCController* m_gcController;

    QFile *m_stdin;

    QList<QObject*> windows;
    bool m_enableTextOutput;
    bool m_singleFileMode;
};

class NetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    NetworkAccessManager(QObject* parent);

private slots:
#ifndef QT_NO_OPENSSL
    void sslErrorsEncountered(QNetworkReply*, const QList<QSslError>&);
#endif
};

class WebPage : public QWebPage {
    Q_OBJECT
public:
    WebPage(QObject* parent, DumpRenderTree*);
    virtual ~WebPage();
    QWebInspector* webInspector();

    QWebPage *createWindow(QWebPage::WebWindowType);

    void javaScriptAlert(QWebFrame *frame, const QString& message);
    void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID);
    bool javaScriptConfirm(QWebFrame *frame, const QString& msg);
    bool javaScriptPrompt(QWebFrame *frame, const QString& msg, const QString& defaultValue, QString* result);

    void resetSettings();

    virtual bool supportsExtension(QWebPage::Extension extension) const;
    virtual bool extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output);

    QObject* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&);

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
    QWebInspector* m_webInspector;
    DumpRenderTree *m_drt;
};

}

#endif
