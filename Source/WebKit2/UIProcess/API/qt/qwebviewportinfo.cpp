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
#include "QtViewportInteractionEngine.h"
#include "qwebviewportinfo_p.h"

#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"

using namespace WebKit;

QWebViewportInfo::QWebViewportInfo(QQuickWebViewPrivate* webViewPrivate, QObject* parent)
    : QObject(parent)
    , m_webViewPrivate(webViewPrivate)
{
}

QWebViewportInfo::~QWebViewportInfo()
{
}

QSize QWebViewportInfo::contentsSize() const
{
    return QSize(m_webViewPrivate->pageView->contentsSize().toSize());
}

QVariant QWebViewportInfo::currentScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->currentCSSScale();

    return m_webViewPrivate->attributes.initialScale;
}

QVariant QWebViewportInfo::devicePixelRatio() const
{
    return m_webViewPrivate->attributes.devicePixelRatio;
}

QVariant QWebViewportInfo::initialScale() const
{
    return m_webViewPrivate->attributes.initialScale;
}

QVariant QWebViewportInfo::minimumScale() const
{
    return m_webViewPrivate->attributes.minimumScale;
}

QVariant QWebViewportInfo::maximumScale() const
{
    return m_webViewPrivate->attributes.maximumScale;
}

QVariant QWebViewportInfo::isScalable() const
{
    return !!m_webViewPrivate->attributes.userScalable;
}

QVariant QWebViewportInfo::layoutSize() const
{
    return QSizeF(m_webViewPrivate->attributes.layoutSize.width(), m_webViewPrivate->attributes.layoutSize.height());
}

void QWebViewportInfo::didUpdateContentsSize()
{
    emit contentsSizeUpdated();
}

void QWebViewportInfo::didUpdateCurrentScale()
{
    emit currentScaleUpdated();
}

void QWebViewportInfo::didUpdateViewportConstraints()
{
    emit viewportConstraintsUpdated();
    emit currentScaleUpdated();
}
