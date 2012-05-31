/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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

#include "config.h"
#include "PlatformVideoWindow.h"
#if ENABLE(VIDEO) && USE(GSTREAMER) && !defined(GST_API_VERSION_1)

#include "HTMLVideoElement.h"
#include "PlatformVideoWindowPrivate.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPalette>

using namespace WebCore;

#ifndef QT_NO_CURSOR
static const int gHideMouseCursorDelay = 3000;
#endif

FullScreenVideoWindow::FullScreenVideoWindow()
    : m_mediaElement(0)
{
#if !HAVE(QT5)
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
#endif
    setWindowModality(Qt::ApplicationModal);

#ifndef QT_NO_CURSOR
    m_cursorTimer.setSingleShot(true);
    connect(&m_cursorTimer, SIGNAL(timeout()), this, SLOT(hideCursor()));
#endif
}

void FullScreenVideoWindow::setVideoElement(HTMLVideoElement* element)
{
    m_mediaElement = element;
}

void FullScreenVideoWindow::keyPressEvent(QKeyEvent* ev)
{
    if (m_mediaElement && ev->key() == Qt::Key_Space) {
        if (!m_mediaElement->paused())
            m_mediaElement->pause();
        else
            m_mediaElement->play();
    } else if (ev->key() == Qt::Key_Escape)
        emit closed();
    Base::keyPressEvent(ev);
}

bool FullScreenVideoWindow::event(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseMove:
        showCursor();
        ev->accept();
        return true;
    case QEvent::MouseButtonDblClick:
        emit closed();
        ev->accept();
        return true;
    case QEvent::Close:
#ifndef QT_NO_CURSOR
        m_cursorTimer.stop();
#endif
#if !HAVE(QT5)
        setMouseTracking(false);
        releaseMouse();
#endif
#ifndef QT_NO_CURSOR
        QGuiApplication::restoreOverrideCursor();
#endif
        break;
    default:
        break;
    }
    return Base::event(ev);
}

void FullScreenVideoWindow::showFullScreen()
{
    Base::showFullScreen();
#if !HAVE(QT5)
    setMouseTracking(true);
#endif
    raise();
#if !HAVE(QT5)
    setFocus();
#endif
    hideCursor();
}

void FullScreenVideoWindow::hideCursor()
{
#ifndef QT_NO_CURSOR
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
#endif
}

void FullScreenVideoWindow::showCursor()
{
#ifndef QT_NO_CURSOR
    QGuiApplication::restoreOverrideCursor();
    m_cursorTimer.start(gHideMouseCursorDelay);
#endif
}


PlatformVideoWindow::PlatformVideoWindow()
{
    Base* win = new FullScreenVideoWindow();
    m_window = win;
    win->setWindowFlags(win->windowFlags() | Qt::FramelessWindowHint);
    // FIXME: Port to Qt 5.
#if !HAVE(QT5)
    QPalette p;
    p.setColor(QPalette::Base, Qt::black);
    p.setColor(QPalette::Window, Qt::black);
    win->setPalette(p);
#endif
    win->showFullScreen();
    m_videoWindowId = win->winId();
}

PlatformVideoWindow::~PlatformVideoWindow()
{
    delete m_window;
    m_videoWindowId = 0;
}

void PlatformVideoWindow::prepareForOverlay(GstMessage*)
{
}
#endif // ENABLE(VIDEO) && USE(GSTREAMER) && !defined(GST_API_VERSION_1)
