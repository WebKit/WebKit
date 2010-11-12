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

#include "TestController.h"

#include "WKStringQt.h"

#include <cstdlib>
#include <QCoreApplication>
#if (QT_VERSION >= 0x040700)
#    include <QElapsedTimer>
#else
#    include <QTime>
#endif
#include <QEventLoop>
#include <QFileInfo>
#include <QLibrary>
#include <QObject>
#include <QtGlobal>
#include <wtf/Platform.h>
#include <wtf/text/WTFString.h>

namespace WTR {

// With a bigger interval we would waste to much time
// after the test had been finished.
static const unsigned kTimerIntervalMS = 1;

class RunUntilConditionLoop : public QObject {
    Q_OBJECT

public:
    static void start(bool& done, int timeout)
    {
        static RunUntilConditionLoop* instance = new RunUntilConditionLoop;
        instance->run(done, timeout);
    }

private:
    RunUntilConditionLoop() {}

    void run(bool& done, int timeout)
    {
        m_condition = &done;
        m_timeout = timeout;
        m_elapsedTime.start();
        m_timerID = startTimer(kTimerIntervalMS);
        ASSERT(m_timerID);
        m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    virtual void timerEvent(QTimerEvent*)
    {
        if (!*m_condition && m_elapsedTime.elapsed() < m_timeout)
            return;

        killTimer(m_timerID);
        m_eventLoop.exit();
    }

    QEventLoop m_eventLoop;
    bool* m_condition;
    int m_timerID;
    int m_timeout;
#if (QT_VERSION >= 0x040700)
    QElapsedTimer m_elapsedTime;
#else
    QTime m_elapsedTime;
#endif
};

void TestController::platformInitialize()
{
}

void TestController::platformRunUntil(bool& done, double timeout)
{
    RunUntilConditionLoop::start(done, static_cast<int>(timeout * 1000));
}

static bool isExistingLibrary(const QString& path)
{
#if OS(WINDOWS) || OS(SYMBIAN)
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
    // This is called after initializeInjectedBundlePath.
    m_testPluginDirectory = m_injectedBundlePath;
}

void TestController::platformInitializeContext()
{
}

#include "TestControllerQt.moc"

} // namespace WTR
