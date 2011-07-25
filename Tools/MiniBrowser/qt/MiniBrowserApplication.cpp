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

#include "utils.h"
#include <QRegExp>
#include <QEvent>
#include <QMouseEvent>
#include <QTouchEvent>

extern Q_GUI_EXPORT void qt_translateRawTouchEvent(QWidget*, QTouchEvent::DeviceType, const QList<QTouchEvent::TouchPoint>&);

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
    : QApplication(argc, argv, QApplication::GuiServer)
    , m_windowOptions()
    , m_spontaneousTouchEventReceived(false)
    , m_sendingFakeTouchEvent(false)
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

    if (!event->spontaneous() || m_sendingFakeTouchEvent || m_spontaneousTouchEventReceived)
        return QApplication::notify(target, event);
    if (isTouchEvent(event) && static_cast<QTouchEvent*>(event)->deviceType() == QTouchEvent::TouchScreen) {
        m_spontaneousTouchEventReceived = true;
        return QApplication::notify(target, event);
    }
    if (isMouseEvent(event)) {
        const QMouseEvent* const mouseEvent = static_cast<QMouseEvent*>(event);

        QTouchEvent::TouchPoint touchPoint;
        touchPoint.setScreenPos(mouseEvent->globalPos());
        touchPoint.setPos(mouseEvent->pos());

        switch (mouseEvent->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            touchPoint.setId(mouseEvent->button());
            if (m_touchPoints.contains(touchPoint.id()))
                touchPoint.setState(Qt::TouchPointMoved);
            else
                touchPoint.setState(Qt::TouchPointPressed);
            break;
        case QEvent::MouseMove:
            if (!mouseEvent->buttons() || !m_touchPoints.contains(mouseEvent->buttons()))
                return QApplication::notify(target, event);
            touchPoint.setState(Qt::TouchPointMoved);
            touchPoint.setId(mouseEvent->buttons());
            break;
        case QEvent::MouseButtonRelease:
            if (mouseEvent->modifiers().testFlag(Qt::ControlModifier))
                return QApplication::notify(target, event);
            touchPoint.setState(Qt::TouchPointReleased);
            touchPoint.setId(mouseEvent->button());
            break;
        default:
            Q_ASSERT_X(false, "multi-touch mocking", "unhandled event type");
        }

        // Update current touch-point
        m_touchPoints.insert(touchPoint.id(), touchPoint);

        // Update states for all other touch-points
        for (QHash<int, QTouchEvent::TouchPoint>::iterator it = m_touchPoints.begin(); it != m_touchPoints.end(); ++it) {
            if (it.value().id() != touchPoint.id())
                it.value().setState(Qt::TouchPointStationary);
        }

        m_sendingFakeTouchEvent = true;
        qt_translateRawTouchEvent(0, QTouchEvent::TouchScreen, m_touchPoints.values());
        m_sendingFakeTouchEvent = false;

        // Get rid of touch-points that are no longer valid
        foreach (const QTouchEvent::TouchPoint& touchPoint, m_touchPoints) {
            if (touchPoint.state() ==  Qt::TouchPointReleased)
                m_touchPoints.remove(touchPoint.id());
        }
    }

    return QApplication::notify(target, event);
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
             << "[-chunked-drawing-area]"
             << "[-print-loaded-urls]"
#if defined(QT_CONFIGURED_WITH_OPENGL)
             << "[-gl-viewport]"
#endif
             << "URLs";
        appQuit(0);
    }

    if (args.contains("-touch"))
        m_windowOptions.useTouchWebView = true;

    if (args.contains("-maximize"))
        m_windowOptions.startMaximized = true;

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
        m_windowOptions.printLoadedUrls = true;

#if defined(QT_CONFIGURED_WITH_OPENGL)
    if (args.contains("-gl-viewport"))
        m_windowOptions.useQGLWidgetViewport = true;
#endif
}
