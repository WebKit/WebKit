/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged.
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

#include "config.h"

#include "TestController.h"
#include "qquickwebview_p.h"

#include <stdio.h>

#include <QApplication>
#include <QObject>
#include <QTimer>

class Launcher : public QObject {
    Q_OBJECT

public:
    Launcher(int argc, char** argv)
        : m_argc(argc)
        , m_argv(argv)
    {
    }

    ~Launcher()
    {
        delete m_controller;
    }

public slots:
    void launch()
    {
        m_controller = new WTR::TestController(m_argc, const_cast<const char**>(m_argv));
        QApplication::exit();
    }

private:
    WTR::TestController* m_controller;
    int m_argc;
    char** m_argv;
};

void messageHandler(QtMsgType type, const char* message)
{
    if (type == QtCriticalMsg) {
        fprintf(stderr, "%s\n", message);
        return;
    }

    // Do nothing
}

int main(int argc, char** argv)
{
#if !defined(NDEBUG) && defined(Q_OS_UNIX)
    if (qgetenv("QT_WEBKIT_PAUSE_UI_PROCESS") == "1") {
        fprintf(stderr, "Pausing UI process, please attach to PID %d and continue... ", getpid());
        pause();
        fprintf(stderr, " OK\n");
    }
#endif

    // Suppress debug output from Qt if not started with --verbose
    bool suppressQtDebugOutput = true;
    for (int i = 1; i < argc; ++i) {
        if (!qstrcmp(argv[i], "--verbose")) {
            suppressQtDebugOutput = false;
            break;
        }
    }

    // Has to be done before QApplication is constructed in case
    // QApplication itself produces debug output.
    if (suppressQtDebugOutput) {
        qInstallMsgHandler(messageHandler);
        if (qgetenv("QT_WEBKIT_SUPPRESS_WEB_PROCESS_OUTPUT").isEmpty())
            qputenv("QT_WEBKIT_SUPPRESS_WEB_PROCESS_OUTPUT", "1");
    }

    qputenv("QT_WEBKIT_THEME_NAME", "qstyle");

    QByteArray platform = qgetenv("QT_QPA_PLATFORM");
    QByteArray platformPluginPath = qgetenv("QT_QPA_PLATFORM_PLUGIN_PATH");
    for (int i = 0; i < argc; ++i) {
        if (QByteArray(argv[i]) == "-platform" && i + 1 < argc)
            platform = argv[i + 1];
        else if (QByteArray(argv[i]) == "-platformpluginpath" && i + 1 < argc)
            platformPluginPath = argv[i + 1];
    }
    if (!platform.isEmpty())
        qputenv("QT_WEBKIT_ORIGINAL_PLATFORM", platform);
    if (!platformPluginPath.isEmpty())
        qputenv("QT_WEBKIT_ORIGINAL_PLATFORM_PLUGIN_PATH", platformPluginPath);

    // Tell the web process that we want to use the test platform plugin.
    qputenv("QT_WEBKIT2_TEST_PLATFORM_PLUGIN_PATH", TEST_PLATFORM_PLUGIN_PATH);

    QQuickWebViewExperimental::setFlickableViewportEnabled(false);
    QApplication app(argc, argv);
    Launcher launcher(argc, argv);
    QTimer::singleShot(0, &launcher, SLOT(launch()));
    return app.exec();;
}

#include "main.moc"
