/*
    Copyright (C) 2007 Trolltech ASA
    Copyright (C) 2007 Staikos Computing Services Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef QWEBFRAME_H
#define QWEBFRAME_H

#include <qobject.h>

#include <qwebkitglobal.h>

class QRect;
class QPoint;
class QPainter;
class QMouseEvent;
class QWheelEvent;

class QWebFramePrivate;
class QWebPage;

namespace WebCore {
    class WidgetPrivate;
    class FrameLoaderClientQt;
}
class QWebFrameData;

class QWEBKIT_EXPORT QWebFrame : public QObject
{
    Q_OBJECT
protected:
    QWebFrame(QWebPage *parent, QWebFrameData *frameData);
    QWebFrame(QWebFrame *parent, QWebFrameData *frameData);
    ~QWebFrame();

public:
    QWebPage *page() const;

    void addToJSWindowObject(const QByteArray &name, QObject *object);
    QString markup() const;
    QString innerText() const;
    QString renderTreeDump() const;
    QString selectedText() const;
    QString title() const;

    QList<QWebFrame*> childFrames() const;

    void suppressScrollbars(bool suppress);

    void render(QPainter *painter, const QRect &source);

    QPoint pos() const;
    QRect geometry() const;

public Q_SLOTS:
    QString evaluateJavaScript(const QString& scriptSource);

signals:
    void cleared();
    void loadDone(bool ok);
    void titleChanged(const QString &title);
    void hoveringOverLink(const QString &link, const QString &title);

protected:
    //for dumprendertree
    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent*);
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mouseReleaseEvent(QMouseEvent*);
    virtual void wheelEvent(QWheelEvent*);

private:
    friend class QWebPage;
    friend class QWebPagePrivate;
    friend class WebCore::WidgetPrivate;
    friend class WebCore::FrameLoaderClientQt;
    QWebFramePrivate *d;
};

#endif
