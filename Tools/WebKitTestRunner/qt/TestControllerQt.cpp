/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestController.h"

#include "WKStringQt.h"

#include <cstdlib>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QLibrary>
#include <QObject>
#include <qquickwebview_p.h>
#include <QtGlobal>
#include <wtf/Platform.h>
#include <wtf/text/WTFString.h>

namespace WTR {

class TestControllerRunLoop : public QObject {
    Q_OBJECT
public:
    static TestControllerRunLoop* instance()
    {
        static TestControllerRunLoop* result = new TestControllerRunLoop;
        return result;
    }

    void start(int msec)
    {
        m_timerID = startTimer(msec);
        ASSERT(m_timerID);
        m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    void stop()
    {
        killTimer(m_timerID);
        m_eventLoop.quit();
    }
private:
    TestControllerRunLoop() {}

    void timerEvent(QTimerEvent*)
    {
        fprintf(stderr, "FAIL: TestControllerRunLoop timed out.\n");
        stop();
    }

    QEventLoop m_eventLoop;
    int m_timerID;
};

void TestController::notifyDone()
{
    TestControllerRunLoop::instance()->stop();
}

void TestController::platformInitialize()
{
    QQuickWebView::platformInitialize();
}

void TestController::platformRunUntil(bool&, double timeout)
{
    TestControllerRunLoop::instance()->start(static_cast<int>(timeout * 1000));
}

static bool isExistingLibrary(const QString& path)
{
#if OS(WINDOWS)
    const char* librarySuffixes[] = { ".dll" };
#elif OS(MAC_OS_X)
    const char* librarySuffixes[] = { ".bundle", ".dylib", ".so" };
#elif OS(UNIX)
    const char* librarySuffixes[] = { ".so" };
#else
#error Library path suffix should be specified for this platform
#endif
    for (unsigned i = 0; i < sizeof(librarySuffixes) / sizeof(const char*); ++i) {
        if (QLibrary::isLibrary(path + librarySuffixes[i]))
            return true;
    }

    return false;
}

void TestController::initializeInjectedBundlePath()
{
    QString path = QLatin1String(getenv("WTR_INJECTEDBUNDLE_PATH"));
    if (path.isEmpty())
        path = QFileInfo(QCoreApplication::applicationDirPath() + "/../lib/libWTRInjectedBundle").absoluteFilePath();
    if (!isExistingLibrary(path))
        qFatal("Cannot find the injected bundle at %s\n", qPrintable(path));

    m_injectedBundlePath = WKStringCreateWithQString(path);
}

void TestController::initializeTestPluginDirectory()
{
    m_testPluginDirectory = WKStringCreateWithUTF8CString(qgetenv("QTWEBKIT_PLUGIN_PATH").constData());
}

void TestController::platformInitializeContext()
{
}

void TestController::runModal(PlatformWebView*)
{
    // FIXME: Need to implement this to test showModalDialog.
}

const char* TestController::platformLibraryPathForTesting()
{
    return 0;
}

#include "TestControllerQt.moc"

} // namespace WTR
