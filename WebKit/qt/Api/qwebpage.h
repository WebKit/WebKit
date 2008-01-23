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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef QWEBPAGE_H
#define QWEBPAGE_H

#include "qwebsettings.h"
#include "qwebkitglobal.h"

#include <QtCore/qobject.h>
#include <QtGui/qwidget.h>
class QNetworkProxy;
class QUndoStack;
class QUrl;
class QWebFrame;
class QWebNetworkRequest;
class QNetworkRequest;
class QNetworkReply;
class QNetworkAccessManager;
class QWebHistory;

class QWebPagePrivate;
class QWebFrameData;
class QWebNetworkInterface;

namespace WebCore {
    class ChromeClientQt;
    class EditorClientQt;
    class FrameLoaderClientQt;
    class FrameLoadRequest;
    class InspectorClientQt;
    class ResourceHandle;
    class HitTestResult;
}

class QWEBKIT_EXPORT QWebPage : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool modified READ isModified)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(QSize viewportSize READ viewportSize WRITE setViewportSize)
public:
    enum NavigationRequestResponse {
        AcceptNavigationRequest,
        IgnoreNavigationRequest
    };

    enum NavigationType {
        NavigationTypeLinkClicked,
        NavigationTypeFormSubmitted,
        NavigationTypeBackForward,
        NavigationTypeReload,
        NavigationTypeFormResubmitted,
        NavigationTypeOther
    };

    enum WebAction {
        NoWebAction = - 1,

        OpenLink,

        OpenLinkInNewWindow,
        OpenFrameInNewWindow,

        DownloadLinkToDisk,
        CopyLinkToClipboard,

        OpenImageInNewWindow,
        DownloadImageToDisk,
        CopyImageToClipboard,

        GoBack, // ###GoBackward instead?
        GoForward,
        Stop,
        Reload,

        Cut,
        Copy,
        Paste,

        Undo,
        Redo,
        MoveToNextChar,
        MoveToPreviousChar,
        MoveToNextWord,
        MoveToPreviousWord,
        MoveToNextLine,
        MoveToPreviousLine,
        MoveToStartOfLine,
        MoveToEndOfLine,
        MoveToStartOfBlock,
        MoveToEndOfBlock,
        MoveToStartOfDocument,
        MoveToEndOfDocument,
        SelectNextChar,
        SelectPreviousChar,
        SelectNextWord,
        SelectPreviousWord,
        SelectNextLine,
        SelectPreviousLine,
        SelectStartOfLine,
        SelectEndOfLine,
        SelectStartOfBlock,
        SelectEndOfBlock,
        SelectStartOfDocument,
        SelectEndOfDocument,
        DeleteStartOfWord,
        DeleteEndOfWord,

        SetTextDirectionDefault,
        SetTextDirectionLeftToRight,
        SetTextDirectionRightToLeft,

        ToggleBold,
        ToggleItalic,
        ToggleUnderline,

        InspectElement,

        WebActionCount
    };


    explicit QWebPage(QObject *parent = 0);
    ~QWebPage();

    QWebFrame *mainFrame() const;
    QWebFrame *currentFrame() const;

    QWebHistory *history() const;

    QWebSettings *settings();

    void setView(QWidget *view);
    QWidget *view() const;

    bool isModified() const;
    QUndoStack *undoStack() const;

#if QT_VERSION < 0x040400
    void setNetworkInterface(QWebNetworkInterface *interface);
    QWebNetworkInterface *networkInterface() const;

    // #### why is this in the page itself?
#ifndef QT_NO_NETWORKPROXY
    void setNetworkProxy(const QNetworkProxy& proxy);
    QNetworkProxy networkProxy() const;
#endif

#else
    void setNetworkAccessManager(QNetworkAccessManager *manager);
    QNetworkAccessManager *networkAccessManager() const;
#endif

    quint64 totalBytes() const;
    quint64 bytesReceived() const;

    QString selectedText() const;

    QAction *action(WebAction action) const;
    virtual void triggerAction(WebAction action, bool checked = false);

    QSize viewportSize() const;
    void setViewportSize(const QSize &size) const;

    virtual bool event(QEvent*);
    virtual bool focusNextPrevChild(bool next);

Q_SIGNALS:
    void loadProgressChanged(int progress);
    void hoveringOverLink(const QString &link, const QString &title, const QString &textContent = QString());
    void statusBarTextChanged(const QString& text);
    void selectionChanged();
    void frameCreated(QWebFrame *frame);
    void geometryChangeRequest(const QRect& geom);

#if QT_VERSION >= 0x040400
    void handleUnsupportedContent(QNetworkReply *reply);
    void download(const QNetworkRequest &request);
#endif

    //void addEmbeddableWidget(QWidget *widget);
    //void addEmbeddableWidget(const QString &classid, QWidget *widget);
    //void removeEmbeddableWidget(QWidget *widget);
    //QHash<QString, QWidget *> embeddableWidgets() const;
    //void clearEmbeddableWidgets();

protected:
    virtual QWebPage *createWindow();
    virtual QWebPage *createModalDialog();
    virtual QObject *createPlugin(const QString &classid, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues);

#if QT_VERSION < 0x040400
    virtual NavigationRequestResponse navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, NavigationType type);
#else
    virtual NavigationRequestResponse navigationRequested(QWebFrame *frame, const QNetworkRequest &request, NavigationType type);
#endif
    virtual QString chooseFile(QWebFrame *originatingFrame, const QString& oldFile);
    virtual void javaScriptAlert(QWebFrame *originatingFrame, const QString& msg);
    virtual bool javaScriptConfirm(QWebFrame *originatingFrame, const QString& msg);
    virtual bool javaScriptPrompt(QWebFrame *originatingFrame, const QString& msg, const QString& defaultValue, QString* result);
    virtual void javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString& sourceID);

    virtual QString userAgentFor(const QUrl& url) const;

private:
    Q_PRIVATE_SLOT(d, void _q_onLoadProgressChanged(int))
    Q_PRIVATE_SLOT(d, void _q_webActionTriggered(bool checked));
    friend class QWebFrame;
    friend class QWebPagePrivate;
    friend class WebCore::ChromeClientQt;
    friend class WebCore::EditorClientQt;
    friend class WebCore::FrameLoaderClientQt;
    friend class WebCore::InspectorClientQt;
    friend class WebCore::ResourceHandle;
    QWebPagePrivate *d;
};



#endif
