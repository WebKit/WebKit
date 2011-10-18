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

#ifndef qtouchwebpage_h
#define qtouchwebpage_h

#include "qwebkitglobal.h"
#include "qwebkittypes.h"

#include <QtDeclarative/qsgitem.h>
#include <QSharedPointer>

class QTouchWebPagePrivate;
class QTouchWebPageProxy;
class QWebNavigationController;
class QWebPreferences;

namespace WebKit {
    class TouchViewInterface;
}

class QWEBKIT_EXPORT QTouchWebPage : public QSGItem {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(QWebNavigationController* navigation READ navigationController CONSTANT FINAL)
    Q_PROPERTY(QWebPreferences* preferences READ preferences CONSTANT FINAL)
    Q_ENUMS(ErrorType)
public:
    enum ErrorType {
        EngineError,
        NetworkError,
        HttpError
    };

    QTouchWebPage(QSGItem* parent = 0);

    virtual ~QTouchWebPage();

    Q_INVOKABLE void load(const QUrl&);
    Q_INVOKABLE QUrl url() const;

    Q_INVOKABLE QString title() const;
    int loadProgress() const;

    QWebNavigationController* navigationController() const;
    QWebPreferences* preferences() const;

    virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*);
    virtual bool event(QEvent*);

Q_SIGNALS:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void loadStarted();
    void loadSucceeded();
    void loadFailed(QTouchWebPage::ErrorType errorType, int errorCode, const QUrl& url);
    void loadProgressChanged(int progress);

protected Q_SLOTS:
    void onVisibleChanged();

protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void touchEvent(QTouchEvent*);

    virtual void geometryChanged(const QRectF&, const QRectF&);

private:
    QTouchWebPagePrivate* d;
    friend class QTouchWebViewPrivate;
    friend class WebKit::TouchViewInterface;
};

#endif /* qtouchwebpage_h */
