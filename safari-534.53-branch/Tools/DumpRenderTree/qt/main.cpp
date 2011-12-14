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
#include <fontconfig/fontconfig.h>
#endif

#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#endif

#include <limits.h>
#include <signal.h>

#if defined(__GLIBC__) && !defined(__UCLIBC__)
#include <execinfo.h>
#endif

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
    fprintf(stderr, "Usage: DumpRenderTree [-v|--pixel-tests] [--stdout output_filename] [-stderr error_filename] filename [filename2..n]\n");
    fprintf(stderr, "Or folder containing test files: DumpRenderTree [-v|--pixel-tests] dirpath\n");
    fflush(stderr);
}

QString get_backtrace() {
    QString s;

#if defined(__GLIBC__) && !defined(__UCLIBC__)
    void* array[256];
    size_t size; /* number of stack frames */

    size = backtrace(array, 256);

    if (!size)
        return s;

    char** strings = backtrace_symbols(array, size);
    for (int i = 0; i < int(size); ++i) {
        s += QString::number(i) +
             QLatin1String(": ") +
             QLatin1String(strings[i]) + QLatin1String("\n");
    }

    if (strings)
        free (strings);
#endif

    return s;
}

#if HAVE(SIGNAL_H)
static NO_RETURN void crashHandler(int sig)
{
    fprintf(stderr, "%s\n", strsignal(sig));
    fprintf(stderr, "%s\n", get_backtrace().toLatin1().constData());
    exit(128 + sig);
}
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);
#endif

#ifdef Q_WS_X11
    FcInit();
    WebCore::DumpRenderTree::initializeFonts();
#endif

    QApplication::setGraphicsSystem("raster");
    QApplication::setStyle(new QWindowsStyle);

    QFont f("Sans Serif");
    f.setPointSize(9);
    f.setWeight(QFont::Normal);
    f.setStyle(QFont::StyleNormal);
    QApplication::setFont(f);

    QApplication app(argc, argv);
#ifdef Q_WS_X11
    QX11Info::setAppDpiY(0, 96);
    QX11Info::setAppDpiX(0, 96);
#endif

#if HAVE(SIGNAL_H)
    signal(SIGILL, crashHandler);    /* 4:   illegal instruction (not reset when caught) */
    signal(SIGTRAP, crashHandler);   /* 5:   trace trap (not reset when caught) */
    signal(SIGFPE, crashHandler);    /* 8:   floating point exception */
    signal(SIGBUS, crashHandler);    /* 10:  bus error */
    signal(SIGSEGV, crashHandler);   /* 11:  segmentation violation */
    signal(SIGSYS, crashHandler);    /* 12:  bad argument to system call */
    signal(SIGPIPE, crashHandler);   /* 13:  write on a pipe with no reader */
    signal(SIGXCPU, crashHandler);   /* 24:  exceeded CPU time limit */
    signal(SIGXFSZ, crashHandler);   /* 25:  exceeded file size limit */
#endif

    QStringList args = app.arguments();
    if (args.count() < 2) {
        printUsage();
        exit(1);
    }

    // Remove the first arguments, it is application name itself
    args.removeAt(0);

    // Suppress debug output from Qt if not started with -v
    int index = args.indexOf(QLatin1String("-v"));
    if (index == -1) 
        qInstallMsgHandler(messageHandler);
    else
        args.removeAt(index);

    WebCore::DumpRenderTree dumper;

    index = args.indexOf(QLatin1String("--pixel-tests"));
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

#ifdef Q_WS_X11
    FcConfigSetCurrent(0);
#endif
}
