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

#ifndef qwebviewportinfp_p_h
#define qwebviewportinfo_p_h

#include "qwebkitglobal.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSize>
#include <QtCore/QVariant>
#include <QtDeclarative/QtDeclarative>

namespace WebCore {
class ViewportAttributes;
}
class QQuickWebViewPrivate;

class QWEBKIT_EXPORT QWebViewportInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QSize contentsSize READ contentsSize NOTIFY contentsSizeUpdated)
    Q_PROPERTY(qreal currentScale READ currentScale NOTIFY currentScaleUpdated)
    Q_PROPERTY(QVariant devicePixelRatio READ devicePixelRatio NOTIFY viewportConstraintsUpdated)
    Q_PROPERTY(QVariant initialScale READ initialScale NOTIFY viewportConstraintsUpdated)
    Q_PROPERTY(QVariant isScalable READ isScalable NOTIFY viewportConstraintsUpdated)
    Q_PROPERTY(QVariant maximumScale READ maximumScale NOTIFY viewportConstraintsUpdated)
    Q_PROPERTY(QVariant minimumScale READ minimumScale NOTIFY viewportConstraintsUpdated)
    Q_PROPERTY(QVariant layoutSize READ layoutSize NOTIFY viewportConstraintsUpdated)

signals:
    void contentsSizeUpdated();
    void currentScaleUpdated();
    void viewportConstraintsUpdated();

public:
    QWebViewportInfo(QQuickWebViewPrivate* webviewPrivate, QObject* parent = 0);
    virtual ~QWebViewportInfo();

    QSize contentsSize() const;
    qreal currentScale() const;
    QVariant devicePixelRatio() const;
    QVariant initialScale() const;
    QVariant isScalable() const;
    QVariant layoutSize() const;
    QVariant maximumScale() const;
    QVariant minimumScale() const;

    void didUpdateContentsSize();
    void didUpdateCurrentScale();
    void didUpdateViewportConstraints();

private:
    QQuickWebViewPrivate* m_webViewPrivate;
};

#endif // qwebviewportinfo_p
