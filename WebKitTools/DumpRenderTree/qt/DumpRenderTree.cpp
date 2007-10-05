/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "DumpRenderTree.h"
#include "jsobjects.h"

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QBoxLayout>
#include <QScrollArea>
#include <QApplication>
#include <QUrl>

#include <qwebpage.h>
#include <qwebframe.h>
#include <qwebsettings.h>

#include <unistd.h>
#include <qdebug.h>

namespace WebCore {

// Choose some default values.
const unsigned int maxViewWidth = 800;
const unsigned int maxViewHeight = 600;

class WebFrame : public QWebFrame {
public:
    WebFrame(QWebPage *parent, QWebFrameData *frameData)
        : QWebFrame(parent, frameData) {}
    WebFrame(QWebFrame *parent, QWebFrameData *frameData)
        : QWebFrame(parent, frameData) {}
};

class WebPage : public QWebPage {
public:
    WebPage(QWidget *parent, DumpRenderTree *drt);

    QWebFrame *createFrame(QWebFrame *parentFrame, QWebFrameData *frameData);
    QWebPage *createWindow();

    void javaScriptAlert(QWebFrame *frame, const QString& message);
    void javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString& sourceID);

private:
    DumpRenderTree *m_drt;
};

WebPage::WebPage(QWidget *parent, DumpRenderTree *drt)
    : QWebPage(parent), m_drt(drt)
{
    QWebSettings s = settings();
    s.setAttribute(QWebSettings::JavascriptCanOpenWindows, true);
    setSettings(s);
}

QWebFrame *WebPage::createFrame(QWebFrame *parentFrame, QWebFrameData *frameData)
{
    if (parentFrame) {
        WebFrame *f = new WebFrame(parentFrame, frameData);
        connect(f, SIGNAL(cleared()), m_drt, SLOT(initJSObjects()));
        connect(f, SIGNAL(provisionalLoad()),
                m_drt->layoutTestController(), SLOT(provisionalLoad()));
        return f;
    }
    WebFrame *f = new WebFrame(this, frameData);
    connect(f, SIGNAL(titleChanged(const QString&)),
            SIGNAL(titleChanged(const QString&)));
    connect(f, SIGNAL(hoveringOverLink(const QString&, const QString&)),
            SIGNAL(hoveringOverLink(const QString&, const QString&)));

    connect(f, SIGNAL(cleared()), m_drt, SLOT(initJSObjects()));
    connect(f, SIGNAL(provisionalLoad()),
            m_drt->layoutTestController(), SLOT(provisionalLoad()));
    connect(f, SIGNAL(loadDone(bool)),
            m_drt->layoutTestController(), SLOT(maybeDump(bool)));

    return f;
}

QWebPage *WebPage::createWindow()
{
    return m_drt->createWindow();
}

void WebPage::javaScriptAlert(QWebFrame *frame, const QString& message)
{
    fprintf(stdout, "ALERT: %s\n", message.toUtf8().constData());
}

void WebPage::javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString&)
{
    fprintf (stdout, "CONSOLE MESSAGE: line %d: %s\n", lineNumber, message.toUtf8().constData());
}

DumpRenderTree::DumpRenderTree()
    : m_stdin(0)
    , m_notifier(0)
{
    m_controller = new LayoutTestController();
    connect(m_controller, SIGNAL(done()), this, SLOT(dump()), Qt::QueuedConnection);

    m_page = new WebPage(0, this);
    m_page->resize(maxViewWidth, maxViewHeight);
    m_page->mainFrame()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_page->mainFrame()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_eventSender = new EventSender(m_page);

    QObject::connect(this, SIGNAL(quit()), qApp, SLOT(quit()), Qt::QueuedConnection);
}

DumpRenderTree::~DumpRenderTree()
{
    delete m_page;

    delete m_stdin;
    delete m_notifier;
}

void DumpRenderTree::open()
{
    if (!m_stdin) {
        m_stdin = new QFile;
        m_stdin->open(stdin, QFile::ReadOnly);
    }

    if (!m_notifier) {
        m_notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read);
        connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readStdin(int)));
    }
}

void DumpRenderTree::open(const QUrl& url)
{
    resetJSObjects();
    m_page->open(url);
}

void DumpRenderTree::readStdin(int /* socket */)
{
    // Read incoming data from stdin...
    QByteArray line = m_stdin->readLine();
    if (line.endsWith('\n'))
        line.truncate(line.size()-1);
    //fprintf(stderr, "\n    opening %s\n", line.constData());
    if (line.isEmpty())
        quit();
    QFileInfo fi(line);
    open(QUrl::fromLocalFile(fi.absoluteFilePath()));
    fflush(stdout);
}

void DumpRenderTree::resetJSObjects()
{
    m_controller->reset();
    foreach(QWidget *widget, windows)
        delete widget;
    windows.clear();
}

void DumpRenderTree::initJSObjects()
{
    QWebFrame *frame = qobject_cast<QWebFrame*>(sender());
    Q_ASSERT(frame);
    frame->addToJSWindowObject("layoutTestController", m_controller);
    frame->addToJSWindowObject("eventSender", m_eventSender);
}


QString DumpRenderTree::dumpFramesAsText(QWebFrame* frame)
{
    if (!frame)
        return QString();

    QString result;
    QWebFrame *parent = qobject_cast<QWebFrame *>(frame->parent());
    if (parent) {
        result.append(QLatin1String("\n--------\nFrame: '"));
        result.append(frame->name());
        result.append(QLatin1String("'\n--------\n"));
    }

    result.append(frame->innerText());
    result.append(QLatin1String("\n"));

    if (m_controller->shouldDumpChildrenAsText()) {
        QList<QWebFrame *> children = frame->childFrames();
        for (int i = 0; i < children.size(); ++i)
            result += dumpFramesAsText(children.at(i));
    }

    return result;
}

void DumpRenderTree::dump()
{
    QWebFrame *frame = m_page->mainFrame();

    //fprintf(stderr, "    Dumping\n");
    if (!m_notifier) {
        // Dump markup in single file mode...
        QString markup = frame->markup();
        fprintf(stdout, "Source:\n\n%s\n", markup.toUtf8().constData());
    }

    // Dump render text...
    QString renderDump;
    if (m_controller->shouldDumpAsText()) {
        renderDump = dumpFramesAsText(frame);
    } else {
        renderDump = frame->renderTreeDump();
    }
    if (renderDump.isEmpty()) {
        printf("ERROR: nil result from %s", m_controller->shouldDumpAsText() ? "[documentElement innerText]" : "[frame renderTreeAsExternalRepresentation]");
    } else {
        fprintf(stdout, "%s", renderDump.toUtf8().constData());
    }

    fprintf(stdout, "#EOF\n");

    fflush(stdout);

    if (!m_notifier) {
        // Exit now in single file mode...
        quit();
    }
}


QWebPage *DumpRenderTree::createWindow()
{
    if (!m_controller->canOpenWindows())
        return 0;
    QWidget *container = new QWidget(0);
    container->resize(0, 0);
    container->move(-1, -1);
    container->hide();
    QWebPage *page = new QWebPage(container);
    windows.append(container);
    return page;
}
    
}

