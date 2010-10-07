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

#include "NotImplemented.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>

namespace WTR {

static const unsigned kTimerIntervalMS = 50;

class RunUntilConditionLoop : public QObject {
    Q_OBJECT

public:
    static void start(bool& done)
    {
        static RunUntilConditionLoop* instance = new RunUntilConditionLoop;
        instance->run(done);
    }

private:
    RunUntilConditionLoop() {}

    void run(bool& done)
    {
        m_condition = &done;
        m_timerID = startTimer(kTimerIntervalMS);
        ASSERT(m_timerID);
        m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    virtual void timerEvent(QTimerEvent*)
    {
        if (!*m_condition)
            return;

        killTimer(m_timerID);
        m_eventLoop.exit();
    }

    QEventLoop m_eventLoop;
    bool* m_condition;
    int m_timerID;
};

void TestController::platformInitialize()
{
}

void TestController::runUntil(bool& done)
{
    RunUntilConditionLoop::start(done);
    ASSERT(done);
}

void TestController::initializeInjectedBundlePath()
{
    notImplemented();
}

void TestController::initializeTestPluginDirectory()
{
    notImplemented();
}

void TestController::platformInitializeContext()
{
    notImplemented();
}

#include "TestControllerQt.moc"

} // namespace WTR
