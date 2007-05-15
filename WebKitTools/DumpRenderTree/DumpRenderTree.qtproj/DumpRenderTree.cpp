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

#include <unistd.h>
#include <qdebug.h>

namespace WebCore {

// Choose some default values.
const unsigned int maxViewWidth = 800;
const unsigned int maxViewHeight = 600;

class WebPage : public QWebPage {
public:
    WebPage(QWidget *parent) : QWebPage(parent) {}
    
    void javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString& sourceID);
};

void WebPage::javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString&)
{
    fprintf (stdout, "CONSOLE MESSAGE: line %d: %s\n", lineNumber, message.toUtf8().constData());
}

DumpRenderTree::DumpRenderTree()
    : m_stdin(0)
    , m_notifier()
    , m_loading(false)
{
    page = new WebPage(0);
    page->resize(maxViewWidth, maxViewHeight);
    frame = page->mainFrame();
    frame->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frame->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frame->setFrameShape(QFrame::NoFrame);

    // hack to force the right size on the frame
    page->layout()->activate();

    
    m_controller = new LayoutTestController();
    QObject::connect(m_controller, SIGNAL(done()), this, SLOT(dump()), Qt::QueuedConnection);
    QObject::connect(this, SIGNAL(quit()), qApp, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(frame, SIGNAL(cleared()), this, SLOT(initJSObjects()));
    QObject::connect(frame, SIGNAL(loadDone(bool)), this, SLOT(maybeDump(bool)));

    m_eventSender = new EventSender(page);
    
//     page->resize(800, 800);
}

DumpRenderTree::~DumpRenderTree()
{
    delete page;

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
    page->open(url);
}

void DumpRenderTree::readStdin(int /* socket */)
{
    if (m_loading)
        fprintf(stderr, "=========================== still loading\n");
    
    // Read incoming data from stdin...
    QByteArray line = m_stdin->readLine();
    if (line.endsWith('\n'))
        line.truncate(line.size()-1);
    //fprintf(stderr, "\n    opening %s\n", line.constData());
    if (line.isEmpty())
        quit();
    open(QUrl(QString(line)));
}

void DumpRenderTree::resetJSObjects()
{
    m_controller->reset();
}

void DumpRenderTree::initJSObjects()
{
    frame->addToJSWindowObject("layoutTestController", m_controller);    
    frame->addToJSWindowObject("eventSender", m_eventSender);    
}

void DumpRenderTree::dump()
{
    //fprintf(stderr, "    Dumping\n");
    if (!m_notifier) {
        // Dump markup in single file mode...
        QString markup = frame->markup();
        fprintf(stdout, "Source:\n\n%s\n", markup.toUtf8().constData());
    }
    
    // Dump render text...
    QString renderDump;
    if (m_controller->shouldDumpAsText()) {
        renderDump = frame->innerText();
        renderDump.append("\n");
    } else {
        renderDump = frame->renderTreeDump();
    }
    if (renderDump.isEmpty()) {
        printf("ERROR: nil result from %s", m_controller->shouldDumpAsText() ? "[documentElement innerText]" : "[frame renderTreeAsExternalRepresentation]\n#EOF\n");
    } else {
        fprintf(stdout, "%s#EOF\n", renderDump.toUtf8().constData());
    }
    fflush(stdout);

    m_loading = false;
    
    if (!m_notifier) {
        // Exit now in single file mode...
        quit();
    }
}
    
void DumpRenderTree::maybeDump(bool ok)
{
    if (!ok || !m_controller->shouldWaitUntilDone()) 
        dump();
}

}

