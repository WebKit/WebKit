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
#include "qwebviewportinfo_p.h"

#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"

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

    return QVariant();
}

QVariant QWebViewportInfo::devicePixelRatio() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->constraints().devicePixelRatio;

    return QVariant();
}

QVariant QWebViewportInfo::initialScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->constraints().initialScale;

    return QVariant();
}

QVariant QWebViewportInfo::minimumScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->constraints().minimumScale;

    return QVariant();
}

QVariant QWebViewportInfo::maximumScale() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->constraints().maximumScale;

    return QVariant();
}

QVariant QWebViewportInfo::isScalable() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->constraints().isUserScalable;

    return QVariant();
}

QVariant QWebViewportInfo::layoutSize() const
{
    if (QtViewportInteractionEngine* interactionEngine = m_webViewPrivate->viewportInteractionEngine())
        return interactionEngine->constraints().layoutSize;

    return QVariant();
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
