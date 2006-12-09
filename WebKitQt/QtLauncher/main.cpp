/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <wtf/Platform.h>

#include <QApplication>
#if PLATFORM(KDE)
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kdebug.h>
#endif

#include <Document.h>
#include <FrameView.h>
#include <ChromeClientQt.h>
#include <ContextMenuClientQt.h>
#include <EditorClientQt.h>
#include <KURL.h>

#include <FrameQt.h>
#include <page/Page.h>

#include <QVBoxLayout>
#include <QDir>

using namespace WebCore;

#if PLATFORM(KDE)
static KCmdLineOptions options[] =
{
    { "+file",        "File to load", 0 },
    KCmdLineLastOption
};
#endif

int main(int argc, char **argv)
{
    QString url = QString("%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
#if PLATFORM(KDE)
    KCmdLineArgs::init(argc, argv, "testunity", "testunity",
                       "unity testcase app", "0.1");
    KCmdLineArgs::addCmdLineOptions(options);
    KApplication app;
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    if (args->count() != 0)
        url = args->arg(0);
#else
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.count() > 1)
        url = args.at(1);
#endif
     
    QWidget topLevel;
    QBoxLayout *l = new QVBoxLayout(&topLevel);
 
    // Initialize WebCore in Qt platform mode...
    Page* page = new Page(new ChromeClientQt(), new ContextMenuClientQt(), new EditorClientQt());
    Frame* frame = new FrameQt(page, 0, new FrameQtClientDefault());

    FrameView* frameView = new FrameView(frame);
    frame->setView(frameView);
    frameView->setParentWidget(&topLevel);

    l->addWidget(frame->view()->qwidget());
    l->activate();
    frame->view()->qwidget()->show();

    topLevel.show();

    QtFrame(frame)->client()->openURL(KURL(url.toLatin1()));
    
    app.exec();
    delete frame;
    return 0;
}
