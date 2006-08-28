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

#include "config.h"
#include "markup.h"
#include "Document.h"
#include "DumpRenderTree.h"
#include "RenderTreeAsText.h"

#include <QTimer>
#include <QBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QScrollArea>

#include <unistd.h>

namespace WebCore {

// Choose some default values.
const unsigned int maxViewWidth = 800;
const unsigned int maxViewHeight = 600;

DumpRenderTree::DumpRenderTree()
    : m_frame(0)
    , m_stdin(0)
    , m_notifier()
{
    m_frame = new FrameQt(0 /* no toplevel widget */);
    m_frame->view()->setScrollBarsMode(ScrollBarAlwaysOff);

    // Reverse calculations in QAbstractScrollArea::maximumViewportSize()
    QScrollArea* area = qobject_cast<QScrollArea*>(m_frame->view()->qwidget());

    unsigned int viewWidth = maxViewWidth + 2 * area->frameWidth();
    unsigned int viewHeight = maxViewHeight + 2 * area->frameWidth();

    area->resize(viewWidth, viewHeight);
}

DumpRenderTree::~DumpRenderTree()
{
    delete m_frame;

    delete m_stdin;
    delete m_notifier;
}

void DumpRenderTree::open()
{
    if (!m_stdin)
        m_stdin = new QTextStream(stdin, IO_ReadOnly);
    
    if (!m_notifier) {
        m_notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read);
        connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readStdin(int)));
    }
}

void DumpRenderTree::open(const KURL& url)
{
    m_frame->openURL(url);

    // Simple poll mechanism, to find out when the page is loaded...
    checkLoaded();
}

void DumpRenderTree::readStdin(int /* socket */)
{
    // Read incoming data from stdin...
    QString line = m_stdin->readLine(); 
    if (!line.isEmpty())
        open(KURL(line.toLatin1()));
}

void DumpRenderTree::checkLoaded()
{
    if(m_frame->isComplete()) {
        if (!m_notifier) {
            // Dump markup in single file mode...
            DeprecatedString markup = createMarkup(m_frame->document());
            fprintf(stdout, "Source:\n\n%s\n", markup.ascii());
        }

        // Dump render text...
        DeprecatedString renderDump = externalRepresentation(m_frame->renderer());
        fprintf(stdout, "%s#EOF\n", renderDump.ascii());
        fflush(stdout);

        if (!m_notifier) {
            // Exit now in single file mode...
            QApplication::exit();
        }
    } else
        QTimer::singleShot(10, this, SLOT(checkLoaded()));
}

}

#include "DumpRenderTree.moc"
