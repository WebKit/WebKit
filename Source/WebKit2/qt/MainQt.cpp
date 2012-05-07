/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged
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

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QPlatformIntegration>
#include <QPlatformIntegrationPlugin>
#include <QPluginLoader>
#include <stdio.h>

namespace WebKit {
Q_DECL_IMPORT int WebProcessMainQt(QGuiApplication*);
Q_DECL_IMPORT void initializeWebKit2Theme();
}

static void messageHandler(QtMsgType type, const char* message)
{
    if (type == QtCriticalMsg) {
        fprintf(stderr, "%s\n", message);
        return;
    }

    // Do nothing
}

static void initializeTestPlatformPluginForWTRIfRequired()
{
    QByteArray pluginPath = qgetenv("QT_WEBKIT2_TEST_PLATFORM_PLUGIN_PATH");
    if (pluginPath.isEmpty())
        return;

    QPluginLoader loader(QFile::decodeName(pluginPath.data()));
    QPlatformIntegrationPlugin* plugin = qobject_cast<QPlatformIntegrationPlugin*>(loader.instance());
    if (!plugin)
        qFatal("cannot initialize test platform plugin\n");

    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", pluginPath);
    qputenv("QT_QPA_PLATFORM", "testplatform");
}

// The framework entry point.
// We call our platform specific entry point directly rather than WebKitMain because it makes little sense
// to reimplement the handling of command line arguments from QApplication.
int main(int argc, char** argv)
{
#if !defined(NDEBUG) && defined(Q_OS_UNIX)
    if (qgetenv("QT_WEBKIT_PAUSE_WEB_PROCESS") == "1" || qgetenv("QT_WEBKIT2_DEBUG") == "1") {
        fprintf(stderr, "Pausing web process, please attach to PID %d and continue... ", getpid());
        pause();
        fprintf(stderr, " OK\n");
    }
#endif

    initializeTestPlatformPluginForWTRIfRequired();
    WebKit::initializeWebKit2Theme();

    // Has to be done before QApplication is constructed in case
    // QApplication itself produces debug output.
    QByteArray suppressOutput = qgetenv("QT_WEBKIT_SUPPRESS_WEB_PROCESS_OUTPUT");
    if (!suppressOutput.isEmpty() && suppressOutput != "0")
        qInstallMsgHandler(messageHandler);

    return WebKit::WebProcessMainQt(new QApplication(argc, argv));
}
