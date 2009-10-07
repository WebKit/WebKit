/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef QGraphicsWebView_h
#define QGraphicsWebView_h

#include "qwebkitglobal.h"
#include <QtCore/qurl.h>
#include <QtGui/qevent.h>
#include <QtGui/qgraphicswidget.h>
#include <QtGui/qicon.h>
#include <QtGui/qpainter.h>
#include <QtNetwork/qnetworkaccessmanager.h>

class QWebPage;
class QWebHistory;
class QWebSettings;

class QGraphicsWebViewPrivate;

class QWEBKIT_EXPORT QGraphicsWebView : public QGraphicsWidget {
    Q_OBJECT

    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

    Q_PROPERTY(QString html READ toHtml WRITE setHtml)
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactivityChanged)

public:
    QGraphicsWebView(QGraphicsItem* parent = 0);
    ~QGraphicsWebView();

    QWebPage* page() const;
    void setPage(QWebPage*);

    QUrl url() const;
    void setUrl(const QUrl&);

    QString title() const;
    QIcon icon() const;

    qreal zoomFactor() const;
    void setZoomFactor(qreal);

    bool isInteractive() const;
    void setInteractive(bool);

    qreal progress() const;

    void load(const QUrl &url);
    void load(const QNetworkRequest& request, QNetworkAccessManager::Operation operation = QNetworkAccessManager::GetOperation, const QByteArray& body = QByteArray());

    QString toHtml() const;
    void setHtml(const QString& html, const QUrl& baseUrl = QUrl());
    // FIXME: Consider rename to setHtml?
    void setContent(const QByteArray& data, const QString& mimeType = QString(), const QUrl& baseUrl = QUrl());

    QWebHistory* history() const;
    QWebSettings* settings() const;

    QString status() const;

    virtual void setGeometry(const QRectF& rect);
    virtual void updateGeometry();
    virtual void paint(QPainter*, const QStyleOptionGraphicsItem* options, QWidget* widget = 0);
    virtual bool event(QEvent*);

public Q_SLOTS:
    void stop();
    void back();
    void forward();
    void reload();

Q_SIGNALS:
    void loadStarted();
    void loadFinished(bool);

    void progressChanged(qreal);
    void interactivityChanged();
    void urlChanged(const QUrl&);
    void titleChanged(const QString&);
    void iconChanged();
    void statusChanged();
    void zoomFactorChanged();

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent*);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent*);
#ifndef QT_NO_WHEELEVENT
    virtual void wheelEvent(QGraphicsSceneWheelEvent*);
#endif
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
#ifndef QT_NO_CONTEXTMENU
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent*);
#endif
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent*);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent*);
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent*);
    virtual void dropEvent(QGraphicsSceneDragDropEvent*);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);
    virtual bool focusNextPrevChild(bool next);

    virtual bool sceneEvent(QEvent*);

private:
    Q_PRIVATE_SLOT(d, void _q_doLoadProgress(int progress))
    Q_PRIVATE_SLOT(d, void _q_doLoadFinished(bool success))
    Q_PRIVATE_SLOT(d, void _q_setStatusBarMessage(const QString& message))

    QGraphicsWebViewPrivate* const d;
    friend class QGraphicsWebViewPrivate;
};

#endif // QGraphicsWebView_h
