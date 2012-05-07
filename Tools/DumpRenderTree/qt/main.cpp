/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "DumpRenderTreeQt.h"

#include "QtInitializeTestFonts.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QPlatformIntegration>
#include <QPlatformIntegrationPlugin>
#include <QPluginLoader>
#endif

#include <wtf/AlwaysInline.h>

#include <qstringlist.h>
#include <qapplication.h>
#include <qurl.h>
#include <qdir.h>
#include <qdebug.h>
#include <qfont.h>
#include <qwebdatabase.h>
#include <qtimer.h>
#include <qwindowsstyle.h>

#ifdef Q_WS_X11
#include <qx11info_x11.h>
#endif

#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#endif

#include <limits.h>
#include <signal.h>

#include <wtf/ExportMacros.h>
#include <wtf/Assertions.h>

void messageHandler(QtMsgType type, const char *message)
{
    if (type == QtCriticalMsg) {
        fprintf(stderr, "%s\n", message);
        return;
    }
    // do nothing
}

// We only support -v or --pixel-tests or --stdout or --stderr or -, all the others will be 
// pass as test case name (even -abc.html is a valid test case name)
bool isOption(const QString& str)
{
    return str == QString("-v") || str == QString("--pixel-tests")
           || str == QString("--stdout") || str == QString("--stderr")
           || str == QString("--timeout") || str == QString("--no-timeout")
           || str == QString("-");
}

QString takeOptionValue(QStringList& arguments, int index)
{
    QString result;

    if (index + 1 < arguments.count() && !isOption(arguments.at(index + 1)))
        result = arguments.takeAt(index + 1);
    arguments.removeAt(index);

    return result;
}

void printUsage()
{
    fprintf(stderr, "Usage: DumpRenderTree [-v|--pixel-tests] [--stdout output_filename] [-stderr error_filename] [--no-timeout] [--timeout timeout_MS] filename [filename2..n]\n");
    fprintf(stderr, "Or folder containing test files: DumpRenderTree [-v|--pixel-tests] dirpath\n");
    fflush(stderr);
}

#if HAVE(SIGNAL_H)
typedef void (*SignalHandler)(int);

static NO_RETURN void crashHandler(int sig)
{
    WTFReportBacktrace();
    exit(128 + sig);
}

static void setupSignalHandlers(SignalHandler handler)
{
    signal(SIGILL, handler);    /* 4:   illegal instruction (not reset when caught) */
    signal(SIGTRAP, handler);   /* 5:   trace trap (not reset when caught) */
    signal(SIGFPE, handler);    /* 8:   floating point exception */
    signal(SIGBUS, handler);    /* 10:  bus error */
    signal(SIGSEGV, handler);   /* 11:  segmentation violation */
    signal(SIGSYS, handler);    /* 12:  bad argument to system call */
    signal(SIGPIPE, handler);   /* 13:  write on a pipe with no reader */
    signal(SIGXCPU, handler);   /* 24:  exceeded CPU time limit */
    signal(SIGXFSZ, handler);   /* 25:  exceeded file size limit */
}

static void WTFCrashHook()
{
    setupSignalHandlers(SIG_DFL);
}
#endif

static void initializeTestPlatformPlugin(int argc, char* argv[] const)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QPluginLoader loader(TEST_PLATFORM_PLUGIN_PATH);
    QPlatformIntegrationPlugin* plugin = qobject_cast<QPlatformIntegrationPlugin*>(loader.instance());
    if (!plugin)
        qFatal("cannot initialize test platform plugin\n");

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

    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", TEST_PLATFORM_PLUGIN_PATH);
    qputenv("QT_QPA_PLATFORM", "testplatform");
#endif
}

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);
#endif

    // Suppress debug output from Qt if not started with -v
    bool suppressQtDebugOutput = true;
    for (int i = 1; i < argc; ++i) {
        if (!qstrcmp(argv[i], "-v")) {
            suppressQtDebugOutput = false;
            break;
        }
    }

    // Has to be done before QApplication is constructed in case
    // QApplication itself produces debug output.
    if (suppressQtDebugOutput)
        qInstallMsgHandler(messageHandler);

    WebKit::initializeTestFonts();

    initializeTestPlatformPlugin(argc, argv);

    QApplication::setGraphicsSystem("raster");
    QApplication::setStyle(new QWindowsStyle);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

#if QT_VERSION <= QT_VERSION_CHECK(5, 0, 0)
#ifdef Q_WS_X11
    QX11Info::setAppDpiY(0, 96);
    QX11Info::setAppDpiX(0, 96);
#endif

   /*
    * QApplication will initialize the default application font based
    * on the application DPI at construction time, which might be
    * different from the DPI we explicitly set using QX11Info above.
    * See: https://bugreports.qt.nokia.com/browse/QTBUG-21603
    *
    * To ensure that the application font DPI matches the application
    * DPI, we override the application font using the font we get from
    * a QWidget, which has already been resolved against the existing
    * default font, but with the correct paint-device DPI.
   */
    QApplication::setFont(QWidget().font());
#else
    QCoreApplication::setAttribute(Qt::AA_Use96Dpi, true);
#endif

#if HAVE(SIGNAL_H)
    setupSignalHandlers(&crashHandler);
    WTFSetCrashHook(&WTFCrashHook);
#endif

    QStringList args = app.arguments();
    if (args.count() < (!suppressQtDebugOutput ? 3 : 2)) {
        printUsage();
        exit(1);
    }

    // Remove the first arguments, it is application name itself
    args.removeAt(0);

    WebCore::DumpRenderTree dumper;

    int index = args.indexOf(QLatin1String("--pixel-tests"));
    if (index != -1) {
        dumper.setDumpPixels(true);
        args.removeAt(index);
    }

    index = args.indexOf(QLatin1String("--stdout"));
    if (index != -1) {
        QString fileName = takeOptionValue(args, index);
        dumper.setRedirectOutputFileName(fileName);
        if (fileName.isEmpty() || !freopen(qPrintable(fileName), "w", stdout)) {
            fprintf(stderr, "STDOUT redirection failed.");
            exit(1);
        }
    }
    index = args.indexOf(QLatin1String("--stderr"));
    if (index != -1) {
        QString fileName = takeOptionValue(args, index);
        dumper.setRedirectErrorFileName(fileName);
        if (!freopen(qPrintable(fileName), "w", stderr)) {
            fprintf(stderr, "STDERR redirection failed.");
            exit(1);
        }
    }
    QWebDatabase::removeAllDatabases();

    index = args.indexOf(QLatin1String("--timeout"));
    if (index != -1) {
        int timeout = takeOptionValue(args, index).toInt();
        dumper.setTimeout(timeout);
        args.removeAt(index);
    }

    index = args.indexOf(QLatin1String("--no-timeout"));
    if (index != -1) {
        dumper.setShouldTimeout(false);
        args.removeAt(index);
    }

    index = args.indexOf(QLatin1String("-"));
    if (index != -1) {
        args.removeAt(index);

        // Continue waiting in STDIN for more test case after process one test case
        QObject::connect(&dumper, SIGNAL(ready()), &dumper, SLOT(readLine()), Qt::QueuedConnection);   

        // Read and only read the first test case, ignore the others 
        if (args.size() > 0) { 
            // Process the argument first
            dumper.processLine(args[0]);
        } else
           QTimer::singleShot(0, &dumper, SLOT(readLine()));
    } else {
        // Go into standalone mode
        // Standalone mode need at least one test case
        if (args.count() < 1) {
            printUsage();
            exit(1);
        }
        dumper.processArgsLine(args);
    }
    return app.exec();
}
