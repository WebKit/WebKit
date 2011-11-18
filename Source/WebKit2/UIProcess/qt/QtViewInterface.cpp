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
#include "QtViewInterface.h"

#include "QtWebError.h"
#include "qquickwebpage_p.h"
#include "qquickwebpage_p_p.h"
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"

#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QQuickView>
#include <QtGui/QDrag>
#include <QtGui/QGuiApplication>

namespace WebKit {

QtViewInterface::QtViewInterface(QQuickWebView* viewportView)
    : m_viewportView(viewportView)
{
    Q_ASSERT(m_viewportView);
}

void QtViewInterface::didChangeStatusText(const QString& newMessage)
{
    emit m_viewportView->statusBarMessageChanged(newMessage);
}

void QtViewInterface::didChangeLoadProgress(int percentageLoaded)
{
    emit m_viewportView->loadProgressChanged(percentageLoaded);
}

void QtViewInterface::didMouseMoveOverElement(const QUrl& linkURL, const QString& linkTitle)
{
    if (linkURL == lastHoveredURL && linkTitle == lastHoveredTitle)
        return;
    lastHoveredURL = linkURL;
    lastHoveredTitle = linkTitle;
    emit m_viewportView->linkHovered(lastHoveredURL, lastHoveredTitle);
}

void QtViewInterface::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (activeMenu)
        activeMenu->hide();

    if (menu->isEmpty())
        return;

    QWindow* window = m_viewportView->canvas();
    if (!window)
        return;

    activeMenu = menu;

    activeMenu->window()->winId(); // Ensure that the menu has a window
    Q_ASSERT(activeMenu->window()->windowHandle());
    activeMenu->window()->windowHandle()->setTransientParent(window);

    QPoint menuPositionInScene = m_viewportView->mapToScene(menu->pos()).toPoint();
    menu->exec(window->mapToGlobal(menuPositionInScene));
    // The last function to get out of exec() clear the local copy.
    if (activeMenu == menu)
        activeMenu.clear();
}

void QtViewInterface::hideContextMenu()
{
    if (activeMenu)
        activeMenu->hide();
}

void QtViewInterface::runJavaScriptAlert(const QString& alertText)
{
    m_viewportView->d_func()->runJavaScriptAlert(alertText);
}

bool QtViewInterface::runJavaScriptConfirm(const QString& message)
{
    return m_viewportView->d_func()->runJavaScriptConfirm(message);
}

QString QtViewInterface::runJavaScriptPrompt(const QString& message, const QString& defaultValue, bool& ok)
{
    return m_viewportView->d_func()->runJavaScriptPrompt(message, defaultValue, ok);
}

void QtViewInterface::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, QtViewInterface::FileChooserType type)
{
    m_viewportView->d_func()->chooseFiles(listenerRef, selectedFileNames, type);
}

}

