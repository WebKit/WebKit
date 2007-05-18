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

#ifndef QWEBPAGE_H
#define QWEBPAGE_H

#include "qwebpagehistory.h"
#include <qwebkitglobal.h>

#include <qwidget.h>
class QWebFrame;
class QUndoStack;
class QUrl;

class QWebPagePrivate;
class QWebFrameData;

namespace WebCore {
    class ChromeClientQt;
    class FrameLoaderClientQt;
    class FrameLoadRequest;
    class EditorClientQt;
}

class QWEBKIT_EXPORT QWebPage : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(bool modified READ isModified)
public:
    QWebPage(QWidget *parent);
    ~QWebPage();


    void open(const QUrl &url);

    QWebFrame *mainFrame() const;

    QWebPageHistory history() const;

    QSize sizeHint() const;

    QString title() const;

    QUrl url() const;

    bool isModified() const;

    virtual void javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString& sourceID);

    virtual void statusTextChanged(const QString& text);

    virtual void runJavaScriptAlert(QWebFrame *frame, const QString& msg);

    QUndoStack *undoStack();
    
    virtual void dragEnterEvent(QDragEnterEvent *);
    virtual void dragLeaveEvent(QDragLeaveEvent *);
    virtual void dragMoveEvent(QDragMoveEvent *);
    virtual void dropEvent(QDropEvent *);

public slots:
    /**
     * Stops loading of the page, if loading.
     */
    void stop();

    void goBack();
    void goForward();
    void goToHistoryItem(const QWebHistoryItem &item);

    virtual void setWindowGeometry(const QRect& geom);

signals:
    /**
     * Signal is emitted when load is started on one of the child
     * frames of the page. The frame on which the load started
     * is passed.
     */
    void loadStarted(QWebFrame *frame);
    /**
     * Signal is emitted when the global progress status changes.
     * It accumulates changes from all the child frames.
     */
    void loadProgressChanged(int progress);
    /**
     * Signal is emitted when load has been finished on one of
     * the child frames of the page. The frame on which the
     * load finished is passed as an argument.
     */
    void loadFinished(QWebFrame *frame);
    /**
     * Signal is emitted when the title of this page has changed.
     * Applies only to the main frame.  Sub-frame titles do not trigger this.
     */
    void titleChanged(const QString& title);
    /**
     * Signal is emitted when the mouse is hovering over a link.
     * The first parameter is the link url, the second is the link title
     * if any. Method is emitter with both empty parameters when the mouse
     * isn't hovering over any link element.
     */
    void hoveringOverLink(const QString &link, const QString &title);

protected:
    virtual QWebFrame *createFrame(QWebFrame *parentFrame, QWebFrameData *frameData);
    virtual QWebPage *createWindow();

private:
    friend class QWebFrame;
    friend class QWebPagePrivate;
    friend class WebCore::ChromeClientQt;
    friend class WebCore::EditorClientQt;
    friend class WebCore::FrameLoaderClientQt;
    QWebPagePrivate *d;
};



#endif
