/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
// Functions and macros that really need to be in QTestLib

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>

#if !defined(TESTS_SOURCE_DIR)
#define TESTS_SOURCE_DIR ""
#endif

/**
 * Starts an event loop that runs until the given signal is received.
 * Optionally the event loop
 * can return earlier on a timeout.
 *
 * \return \p true if the requested signal was received
 *         \p false on timeout
 */
static bool waitForSignal(QObject* obj, const char* signal, int timeout = 10000)
{
    QEventLoop loop;
    QObject::connect(obj, signal, &loop, SLOT(quit()));
    QTimer timer;
    QSignalSpy timeoutSpy(&timer, SIGNAL(timeout()));
    if (timeout > 0) {
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.setSingleShot(true);
        timer.start(timeout);
    }
    loop.exec();
    return timeoutSpy.isEmpty();
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// Will try to wait for the condition while allowing event processing
#define QTRY_VERIFY(__expr) \
    do { \
        const int __step = 50; \
        const int __timeout = 5000; \
        if (!(__expr)) { \
            QTest::qWait(0); \
        } \
        for (int __i = 0; __i < __timeout && !(__expr); __i+=__step) { \
            QTest::qWait(__step); \
        } \
        QVERIFY(__expr); \
    } while(0)

// Will try to wait for the condition while allowing event processing
#define QTRY_COMPARE(__expr, __expected) \
    do { \
        const int __step = 50; \
        const int __timeout = 5000; \
        if ((__expr) != (__expected)) { \
            QTest::qWait(0); \
        } \
        for (int __i = 0; __i < __timeout && ((__expr) != (__expected)); __i+=__step) { \
            QTest::qWait(__step); \
        } \
        QCOMPARE(__expr, __expected); \
    } while(0)

// Compatibility for Qt5
#define W_QSKIP(a, b) QSKIP(a, b)
#else
#define W_QSKIP(a, b) QSKIP(a)
#endif
