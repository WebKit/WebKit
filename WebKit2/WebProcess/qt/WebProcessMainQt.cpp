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

#include "RunLoop.h"
#include <runtime/InitializeThreading.h>
#include "WebProcess.h"
#include <wtf/Threading.h>

#include <QApplication>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#if USE(MEEGOTOUCH)
#include <MComponentData>
#endif

#if !defined(QWEBKIT_EXPORT)
#if defined(QT_SHARED)
#define QWEBKIT_EXPORT Q_DECL_EXPORT
#else
#define QWEBKIT_EXPORT
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


QWEBKIT_EXPORT int WebProcessMainQt(int argc, char** argv)
{
    QApplication::setGraphicsSystem("raster");
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

    srandom(time(0));

    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    // Create the connection.
    QString identifier(app->arguments().size() > 1 ? app->arguments().at(1) : "");
    WebKit::WebProcess::shared().initialize(identifier, RunLoop::main());

    RunLoop::run();

    // FIXME: Do more cleanup here.

    return 0;
}

}
