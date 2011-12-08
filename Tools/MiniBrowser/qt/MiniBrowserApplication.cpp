/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
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

#include "MiniBrowserApplication.h"

#include "BrowserWindow.h"
#include "utils.h"
#include <QRegExp>
#include <QEvent>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QApplication>

static inline bool isTouchEvent(const QEvent* event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        return true;
    default:
        return false;
    }
}

static inline bool isMouseEvent(const QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
        return true;
    default:
        return false;
    }
}

MiniBrowserApplication::MiniBrowserApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_windowOptions(this)
    , m_realTouchEventReceived(false)
    , m_pendingFakeTouchEventCount(0)
    , m_isRobotized(false)
    , m_robotTimeoutSeconds(0)
    , m_robotExtraTimeSeconds(0)
{
    setOrganizationName("Nokia");
    setApplicationName("QtMiniBrowser");
    setApplicationVersion("0.1");

    handleUserOptions();
}

bool MiniBrowserApplication::notify(QObject* target, QEvent* event)
{
    // We try to be smart, if we received real touch event, we are probably on a device
    // with touch screen, and we should not have touch mocking.

    if (!event->spontaneous() || m_realTouchEventReceived)
        return QApplication::notify(target, event);

    if (isTouchEvent(event) && static_cast<QTouchEvent*>(event)->deviceType() == QTouchEvent::TouchScreen) {
        if (m_pendingFakeTouchEventCount)
            --m_pendingFakeTouchEventCount;
        else
            m_realTouchEventReceived = true;
        return QApplication::notify(target, event);
    }

    BrowserWindow* browserWindow = qobject_cast<BrowserWindow*>(target);
    Q_ASSERT(browserWindow);
    if (event->type() == QEvent::KeyRelease && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Control) {
        foreach (int id, m_heldTouchPoints)
            if (m_touchPoints.contains(id))
                m_touchPoints[id].state = Qt::TouchPointReleased;
        m_heldTouchPoints.clear();
        sendTouchEvent(browserWindow);
    }

    if (browserWindow && isMouseEvent(event)) {
        const QMouseEvent* const mouseEvent = static_cast<QMouseEvent*>(event);

        QWindowSystemInterface::TouchPoint touchPoint;
        touchPoint.area = QRectF(mouseEvent->globalPos(), QSizeF(1, 1));
        touchPoint.pressure = 1;
        touchPoint.isPrimary = false;

        switch (mouseEvent->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            touchPoint.id = mouseEvent->button();
            if (m_touchPoints.contains(touchPoint.id))
                touchPoint.state = Qt::TouchPointMoved;
            else
                touchPoint.state = Qt::TouchPointPressed;
            break;
        case QEvent::MouseMove:
            if (!mouseEvent->buttons() || !m_touchPoints.contains(mouseEvent->buttons()))
                return QApplication::notify(target, event);
            touchPoint.id = mouseEvent->buttons();
            touchPoint.state = Qt::TouchPointMoved;
            break;
        case QEvent::MouseButtonRelease:
            touchPoint.state = Qt::TouchPointReleased;
            touchPoint.id = mouseEvent->button();
            if (mouseEvent->modifiers().testFlag(Qt::ControlModifier)) {
                m_heldTouchPoints.insert(touchPoint.id);
                return QApplication::notify(target, event);
            }
            break;
        default:
            Q_ASSERT_X(false, "multi-touch mocking", "unhandled event type");
        }

        // Update current touch-point
        if (m_touchPoints.isEmpty())
            touchPoint.isPrimary = true;
        m_touchPoints.insert(touchPoint.id, touchPoint);

        // Update states for all other touch-points
        for (QHash<int, QWindowSystemInterface::TouchPoint>::iterator it = m_touchPoints.begin(), end = m_touchPoints.end(); it != end; ++it) {
            if (it.value().id != touchPoint.id)
                it.value().state = Qt::TouchPointStationary;
        }

        sendTouchEvent(browserWindow);
    }

    return QApplication::notify(target, event);
}

void MiniBrowserApplication::sendTouchEvent(BrowserWindow* browserWindow)
{
    m_pendingFakeTouchEventCount++;
    QWindowSystemInterface::handleTouchEvent(browserWindow, QEvent::None, QTouchEvent::TouchScreen, m_touchPoints.values());

    browserWindow->updateVisualMockTouchPoints(m_touchPoints.values());

    // Get rid of touch-points that are no longer valid
    foreach (const QWindowSystemInterface::TouchPoint& touchPoint, m_touchPoints) {
    if (touchPoint.state ==  Qt::TouchPointReleased)
        m_touchPoints.remove(touchPoint.id);
    }
}

static void printHelp(const QString& programName)
{
    qDebug() << "Usage:" << programName.toLatin1().data()
         << "[--desktop]"
         << "[-r list]"
         << "[--robot-timeout seconds]"
         << "[--robot-extra-time seconds]"
         << "[--window-size (width)x(height)]"
         << "[--maximize]"
         << "[-f]                                    Full screen mode."
         << "[-v]"
         << "URL";
}

void MiniBrowserApplication::handleUserOptions()
{
    QStringList args = arguments();
    QFileInfo program(args.takeAt(0));
    QString programName("MiniBrowser");
    if (program.exists())
        programName = program.baseName();

    if (takeOptionFlag(&args, "--help")) {
        printHelp(programName);
        appQuit(0);
    }

    m_windowOptions.setUseTraditionalDesktopBehavior(takeOptionFlag(&args, "--desktop"));
    m_windowOptions.setPrintLoadedUrls(takeOptionFlag(&args, "-v"));
    m_windowOptions.setStartMaximized(takeOptionFlag(&args, "--maximize"));
    m_windowOptions.setStartFullScreen(takeOptionFlag(&args, "-f"));

    if (args.contains("--window-size")) {
        QString value = takeOptionValue(&args, "--window-size");
        QStringList list = value.split(QRegExp("\\D+"), QString::SkipEmptyParts);
        if (list.length() == 2)
            m_windowOptions.setRequestedWindowSize(QSize(list.at(0).toInt(), list.at(1).toInt()));
    }

    if (args.contains("-r")) {
        QString listFile = takeOptionValue(&args, "-r");
        if (listFile.isEmpty())
            appQuit(1, "-r needs a list file to start in robotized mode");
        if (!QFile::exists(listFile))
            appQuit(1, "The list file supplied to -r does not exist.");

        m_isRobotized = true;
        m_urls = QStringList(listFile);

        // toInt() returns 0 if it fails parsing.
        m_robotTimeoutSeconds = takeOptionValue(&args, "--robot-timeout").toInt();
        m_robotExtraTimeSeconds = takeOptionValue(&args, "--robot-extra-time").toInt();
    } else {
        int urlArg = args.indexOf(QRegExp("^[^-].*"));
        if (urlArg != -1)
            m_urls += args.takeAt(urlArg);
    }

    if (!args.isEmpty()) {
        printHelp(programName);
        appQuit(1, QString("Unknown argument(s): %1").arg(args.join(",")));
    }
}
