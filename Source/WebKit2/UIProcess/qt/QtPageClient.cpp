/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QtPageClient.h"

#include "QtWebPageEventHandler.h"
#include "QtWebPageProxy.h"
#include "QtWebUndoCommand.h"
#include "WebContextMenuProxyQt.h"
#include "WebEditCommandProxy.h"
#include "WebPopupMenuProxyQtDesktop.h"
#include <QGuiApplication>
#include <QUndoStack>
#include <WebCore/Cursor.h>
#include <WebCore/DragData.h>
#include <WebCore/FloatRect.h>
#include <WebCore/NotImplemented.h>

using namespace WebKit;
using namespace WebCore;

QtPageClient::QtPageClient()
    : m_qtWebPageProxy(0)
{
}

QtPageClient::~QtPageClient()
{
}

PassOwnPtr<DrawingAreaProxy> QtPageClient::createDrawingAreaProxy()
{
    return m_qtWebPageProxy->createDrawingAreaProxy();
}

void QtPageClient::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    m_qtWebPageProxy->setViewNeedsDisplay(rect);
}

void QtPageClient::pageDidRequestScroll(const IntPoint& pos)
{
    m_qtWebPageProxy->pageDidRequestScroll(pos);
}

void QtPageClient::processDidCrash()
{
    m_qtWebPageProxy->processDidCrash();
}

void QtPageClient::didRelaunchProcess()
{
    m_qtWebPageProxy->didRelaunchProcess();
}

void QtPageClient::didChangeContentsSize(const IntSize& newSize)
{
    m_qtWebPageProxy->didChangeContentsSize(newSize);
}

void QtPageClient::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    m_qtWebPageProxy->didChangeViewportProperties(args);
}

void QtPageClient::startDrag(const WebCore::DragData& dragData, PassRefPtr<ShareableBitmap> dragImage)
{
    m_qtWebPageProxy->startDrag(dragData, dragImage);
}

void QtPageClient::handleDownloadRequest(DownloadProxy* download)
{
    m_qtWebPageProxy->handleDownloadRequest(download);
}

void QtPageClient::setCursor(const WebCore::Cursor& cursor)
{
    // FIXME: This is a temporary fix until we get cursor support in QML items.
    QGuiApplication::setOverrideCursor(*cursor.platformCursor());
}

void QtPageClient::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    notImplemented();
}

void QtPageClient::toolTipChanged(const String&, const String& newTooltip)
{
    // There is not yet any UI defined for the tooltips for mobile so we ignore the change.
}

void QtPageClient::registerEditCommand(PassRefPtr<WebEditCommandProxy> command, WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_qtWebPageProxy->registerEditCommand(command, undoOrRedo);
}

void QtPageClient::clearAllEditCommands()
{
    m_qtWebPageProxy->clearAllEditCommands();
}

bool QtPageClient::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return m_qtWebPageProxy->canUndoRedo(undoOrRedo);
}

void QtPageClient::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_qtWebPageProxy->executeUndoRedo(undoOrRedo);
}

FloatRect QtPageClient::convertToDeviceSpace(const FloatRect& rect)
{
    return rect;
}

FloatRect QtPageClient::convertToUserSpace(const FloatRect& rect)
{
    return rect;
}

IntPoint QtPageClient::screenToWindow(const IntPoint& point)
{
    return point;
}

IntRect QtPageClient::windowToScreen(const IntRect& rect)
{
    return rect;
}

PassRefPtr<WebPopupMenuProxy> QtPageClient::createPopupMenuProxy(WebPageProxy* webPageProxy)
{
    return m_qtWebPageProxy->createPopupMenuProxy(webPageProxy);
}

PassRefPtr<WebContextMenuProxy> QtPageClient::createContextMenuProxy(WebPageProxy*)
{
    return WebContextMenuProxyQt::create(m_qtWebPageProxy);
}

void QtPageClient::flashBackingStoreUpdates(const Vector<IntRect>&)
{
    notImplemented();
}

void QtPageClient::didFindZoomableArea(const IntPoint& target, const IntRect& area)
{
    ASSERT(m_eventHandler);
    m_eventHandler->didFindZoomableArea(target, area);
}

void QtPageClient::didReceiveMessageFromNavigatorQtObject(const String& message)
{
    m_qtWebPageProxy->didReceiveMessageFromNavigatorQtObject(message);
}

#if ENABLE(TOUCH_EVENTS)
void QtPageClient::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    ASSERT(m_eventHandler);
    m_eventHandler->doneWithTouchEvent(event, wasEventHandled);
}
#endif

void QtPageClient::displayView()
{
    // FIXME: Implement.
}

void QtPageClient::scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    // FIXME: Implement.
}

WebCore::IntSize QtPageClient::viewSize()
{
    return m_qtWebPageProxy->viewSize();
}

bool QtPageClient::isViewWindowActive()
{
    // FIXME: The scene graph does not have the concept of being active or not when this was written.
    return true;
}

bool QtPageClient::isViewFocused()
{
    if (!m_qtWebPageProxy)
        return false;

    return m_qtWebPageProxy->isViewFocused();
}

bool QtPageClient::isViewVisible()
{
    if (!m_qtWebPageProxy)
        return false;

    return m_qtWebPageProxy->isViewVisible();
}

bool QtPageClient::isViewInWindow()
{
    // FIXME: Implement.
    return true;
}

void QtPageClient::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    // FIXME: Implement.
}

void QtPageClient::exitAcceleratedCompositingMode()
{
    // FIXME: Implement.
}

