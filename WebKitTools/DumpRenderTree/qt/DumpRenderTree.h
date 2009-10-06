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
#include <QObject>
#include <QTextStream>
#include <QSocketNotifier>

QT_BEGIN_NAMESPACE
class QUrl;
class QFile;
QT_END_NAMESPACE
class QWebPage;
class QWebFrame;

class LayoutTestController;
class EventSender;
class TextInputController;
class GCController;

namespace WebCore {

class DumpRenderTree : public QObject {
Q_OBJECT

public:
    DumpRenderTree();
    virtual ~DumpRenderTree();

    // Initialize in multi-file mode, used by run-webkit-tests.
    void open();

    // Initialize in single-file mode.
    void open(const QUrl& url);

    void setDumpPixels(bool);

    void closeRemainingWindows();
    void resetToConsistentStateBeforeTesting();

    LayoutTestController *layoutTestController() const { return m_controller; }
    EventSender *eventSender() const { return m_eventSender; }
    TextInputController *textInputController() const { return m_textInputController; }

    QWebPage *createWindow();
    int windowCount() const;

    QWebPage *webPage() const { return m_page; }


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

    QWebPage *m_page;

    EventSender *m_eventSender;
    TextInputController *m_textInputController;
    GCController* m_gcController;

    QFile *m_stdin;
    QSocketNotifier* m_notifier;

    QList<QWidget *> windows;
};

}

#endif
