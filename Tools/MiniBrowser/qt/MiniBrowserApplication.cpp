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

#include "qdesktopwebview.h"
#include "qtouchwebview.h"
#include "qwebnavigationcontroller.h"
#include "utils.h"
#include <QRegExp>
#include <QEvent>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QGuiApplication>

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
    : QGuiApplication(argc, argv)
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
        return QGuiApplication::notify(target, event);

    if (isTouchEvent(event) && static_cast<QTouchEvent*>(event)->deviceType() == QTouchEvent::TouchScreen) {
        if (m_pendingFakeTouchEventCount)
            --m_pendingFakeTouchEventCount;
        else
            m_realTouchEventReceived = true;
        return QGuiApplication::notify(target, event);
    }

    QWindow* targetWindow = qobject_cast<QWindow*>(target);
    if (targetWindow && isMouseEvent(event)) {
        const QMouseEvent* const mouseEvent = static_cast<QMouseEvent*>(event);

        QWindowSystemInterface::TouchPoint touchPoint;
        touchPoint.area = QRectF(mouseEvent->globalPos(), QSizeF(1, 1));
        touchPoint.pressure = 1;

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
                return QGuiApplication::notify(target, event);
            touchPoint.state = Qt::TouchPointMoved;
            touchPoint.id = mouseEvent->buttons();
            break;
        case QEvent::MouseButtonRelease:
            if (mouseEvent->modifiers().testFlag(Qt::ControlModifier))
                return QGuiApplication::notify(target, event);
            touchPoint.state = Qt::TouchPointReleased;
            touchPoint.id = mouseEvent->button();
            break;
        default:
            Q_ASSERT_X(false, "multi-touch mocking", "unhandled event type");
        }

        // Update current touch-point
        m_touchPoints.insert(touchPoint.id, touchPoint);

        // Update states for all other touch-points
        for (QHash<int, QWindowSystemInterface::TouchPoint>::iterator it = m_touchPoints.begin(), end = m_touchPoints.end(); it != end; ++it) {
            if (it.value().id != touchPoint.id)
                it.value().state = Qt::TouchPointStationary;
        }

        QList<QWindowSystemInterface::TouchPoint> touchPoints = m_touchPoints.values();
        QWindowSystemInterface::TouchPoint& firstPoint = touchPoints.first();
        firstPoint.isPrimary = true;

        QEvent::Type eventType;
        switch (touchPoint.state) {
        case Qt::TouchPointPressed:
            eventType = QEvent::TouchBegin;
            break;
        case Qt::TouchPointReleased:
            eventType = QEvent::TouchEnd;
            break;
        case Qt::TouchPointStationary:
            // Don't send the event if nothing changed.
            return QGuiApplication::notify(target, event);
        default:
            eventType = QEvent::TouchUpdate;
            break;
        }

        m_pendingFakeTouchEventCount++;
        QWindowSystemInterface::handleTouchEvent(targetWindow, eventType, QTouchEvent::TouchScreen, touchPoints);

        // Get rid of touch-points that are no longer valid
        foreach (const QWindowSystemInterface::TouchPoint& touchPoint, m_touchPoints) {
            if (touchPoint.state ==  Qt::TouchPointReleased)
                m_touchPoints.remove(touchPoint.id);
        }
    }

    return QGuiApplication::notify(target, event);
}

void MiniBrowserApplication::handleUserOptions()
{
    QStringList args = arguments();
    QFileInfo program(args.at(0));
    QString programName("MiniBrowser");
    if (program.exists())
        programName = program.baseName();

    if (args.contains("-help")) {
        qDebug() << "Usage:" << programName.toLatin1().data()
             << "[-touch]"
             << "[-maximize]"
             << "[-r list]"
             << "[-robot-timeout seconds]"
             << "[-robot-extra-time seconds]"
             << "[-print-loaded-urls]"
             << "URLs";
        appQuit(0);
    }

    if (args.contains("-touch"))
        m_windowOptions.setUseTouchWebView(true);

    if (args.contains("-maximize"))
        m_windowOptions.setStartMaximized(true);

    int robotIndex = args.indexOf("-r");
    if (robotIndex != -1) {
        QString listFile = takeOptionValue(&args, robotIndex);
        if (listFile.isEmpty())
            appQuit(1, "-r needs a list file to start in robotized mode");
        if (!QFile::exists(listFile))
            appQuit(1, "The list file supplied to -r does not exist.");

        m_isRobotized = true;
        m_urls = QStringList(listFile);
    } else {
        int lastArg = args.lastIndexOf(QRegExp("^-.*"));
        m_urls = (lastArg != -1) ? args.mid(++lastArg) : args.mid(1);
    }

    int robotTimeoutIndex = args.indexOf("-robot-timeout");
    if (robotTimeoutIndex != -1)
        m_robotTimeoutSeconds = takeOptionValue(&args, robotTimeoutIndex).toInt();

    int robotExtraTimeIndex = args.indexOf("-robot-extra-time");
    if (robotExtraTimeIndex != -1)
        m_robotExtraTimeSeconds = takeOptionValue(&args, robotExtraTimeIndex).toInt();

    if (args.contains("-print-loaded-urls"))
        m_windowOptions.setPrintLoadedUrls(true);
}
