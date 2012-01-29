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

#ifndef qquickwebview_p_h
#define qquickwebview_p_h

#include "qquickurlschemedelegate_p.h"
#include "qwebkitglobal.h"
#include <QtDeclarative/qdeclarativelist.h>
#include <QtQuick/qquickitem.h>

class QWebNavigationRequest;
class QDeclarativeComponent;
class QQuickWebPage;
class QQuickWebViewAttached;
class QQuickWebViewPrivate;
class QQuickWebViewExperimental;
class QWebDownloadItem;
class QWebNavigationHistory;
class QWebPreferences;
class QWebPermissionRequest;
class QWebViewportInfo;
class QQuickNetworkReply;

namespace WTR {
class PlatformWebView;
}

namespace WebKit {
class QtRefCountedNetworkRequestData;
}

namespace WTF {
template<class T> class PassRefPtr;
}

typedef const struct OpaqueWKContext* WKContextRef;
typedef const struct OpaqueWKPageGroup* WKPageGroupRef;
typedef const struct OpaqueWKPage* WKPageRef;

QT_BEGIN_NAMESPACE
class QPainter;
class QUrl;
QT_END_NAMESPACE

class QWEBKIT_EXPORT QQuickWebView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(QUrl icon READ icon NOTIFY iconChanged FINAL)
    Q_PROPERTY(int loadProgress READ loadProgress NOTIFY loadProgressChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY navigationStateChanged FINAL)
    Q_PROPERTY(bool canReload READ canReload NOTIFY navigationStateChanged FINAL)
    Q_ENUMS(NavigationRequestAction)
    Q_ENUMS(ErrorDomain)

public:
    enum NavigationRequestAction {
        AcceptRequest,
        IgnoreRequest
    };

    enum ErrorDomain {
        InternalErrorDomain,
        NetworkErrorDomain,
        HttpErrorDomain,
        DownloadErrorDomain
    };
    QQuickWebView(QQuickItem* parent = 0);
    virtual ~QQuickWebView();

    QUrl url() const;
    QUrl icon() const;
    QString title() const;
    int loadProgress() const;

    bool canGoBack() const;
    bool canGoForward() const;
    bool loading() const;
    bool canReload() const;

    virtual QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

    QPointF mapToWebContent(const QPointF&) const;
    QRectF mapRectToWebContent(const QRectF&) const;
    QPointF mapFromWebContent(const QPointF&) const;
    QRectF mapRectFromWebContent(const QRectF&) const;

    QQuickWebPage* page();

    QQuickWebViewExperimental* experimental() const;
    static QQuickWebViewAttached* qmlAttachedProperties(QObject*);

    static void platformInitialize(); // Only needed by WTR.

public Q_SLOTS:
    void load(const QUrl&);
    void loadHtml(const QString& html, const QUrl& baseUrl = QUrl());

    void goBack();
    void goForward();
    void stop();
    void reload();

Q_SIGNALS:
    void titleChanged(const QString& title);
    void loadStarted();
    void loadSucceeded();
    void loadFailed(QQuickWebView::ErrorDomain errorDomain, int errorCode, const QUrl& url, const QString& description);
    void loadProgressChanged(int progress);
    void urlChanged(const QUrl& url);
    void iconChanged(const QUrl& iconURL);
    void linkHovered(const QUrl& url, const QString& title);
    void navigationStateChanged();
    void navigationRequested(QWebNavigationRequest* request);

protected:
    virtual void geometryChanged(const QRectF&, const QRectF&);
    virtual void componentComplete();
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void touchEvent(QTouchEvent*);
    virtual void mousePressEvent(QMouseEvent*);
    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void mouseReleaseEvent(QMouseEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void wheelEvent(QWheelEvent*);
    virtual void hoverEnterEvent(QHoverEvent*);
    virtual void hoverMoveEvent(QHoverEvent*);
    virtual void hoverLeaveEvent(QHoverEvent*);
    virtual void dragMoveEvent(QDragMoveEvent*);
    virtual void dragEnterEvent(QDragEnterEvent*);
    virtual void dragLeaveEvent(QDragLeaveEvent*);
    virtual void dropEvent(QDropEvent*);
    virtual bool event(QEvent*);

private:
    Q_DECLARE_PRIVATE(QQuickWebView)

    QQuickWebView(WKContextRef, WKPageGroupRef, QQuickItem* parent = 0);
    WKPageRef pageRef() const;

    Q_PRIVATE_SLOT(d_func(), void _q_suspend());
    Q_PRIVATE_SLOT(d_func(), void _q_resume());
    Q_PRIVATE_SLOT(d_func(), void _q_viewportTrajectoryVectorChanged(const QPointF&));
    Q_PRIVATE_SLOT(d_func(), void _q_onOpenPanelFilesSelected());
    Q_PRIVATE_SLOT(d_func(), void _q_onOpenPanelFinished(int result));
    Q_PRIVATE_SLOT(d_func(), void _q_onVisibleChanged());
    Q_PRIVATE_SLOT(d_func(), void _q_onReceivedResponseFromDownload(QWebDownloadItem*));
    Q_PRIVATE_SLOT(d_func(), void _q_onIconChangedForPageURL(const QUrl&, const QUrl&));
    // Hides QObject::d_ptr allowing us to use the convenience macros.
    QScopedPointer<QQuickWebViewPrivate> d_ptr;
    QQuickWebViewExperimental* m_experimental;

    friend class QtWebPageLoadClient;
    friend class QtWebPagePolicyClient;
    friend class QtWebPageUIClient;
    friend class WTR::PlatformWebView;
    friend class QQuickWebViewExperimental;
};

QML_DECLARE_TYPE(QQuickWebView)

class QWEBKIT_EXPORT QQuickWebViewAttached : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWebView* view READ view NOTIFY viewChanged FINAL)

public:
    QQuickWebViewAttached(QObject* object);
    QQuickWebView* view() const { return m_view; }
    void setView(QQuickWebView*);

Q_SIGNALS:
    void viewChanged();

private:
    QQuickWebView* m_view;
};

QML_DECLARE_TYPEINFO(QQuickWebView, QML_HAS_ATTACHED_PROPERTIES)

class QWEBKIT_EXPORT QQuickWebViewExperimental : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWebPage* page READ page CONSTANT FINAL)
    Q_PROPERTY(QWebNavigationHistory* navigationHistory READ navigationHistory CONSTANT FINAL)
    Q_PROPERTY(QDeclarativeComponent* alertDialog READ alertDialog WRITE setAlertDialog NOTIFY alertDialogChanged)
    Q_PROPERTY(QDeclarativeComponent* confirmDialog READ confirmDialog WRITE setConfirmDialog NOTIFY confirmDialogChanged)
    Q_PROPERTY(QDeclarativeComponent* promptDialog READ promptDialog WRITE setPromptDialog NOTIFY promptDialogChanged)
    Q_PROPERTY(QDeclarativeComponent* authenticationDialog READ authenticationDialog WRITE setAuthenticationDialog NOTIFY authenticationDialogChanged)
    Q_PROPERTY(QDeclarativeComponent* certificateVerificationDialog READ certificateVerificationDialog WRITE setCertificateVerificationDialog NOTIFY certificateVerificationDialogChanged)
    Q_PROPERTY(QDeclarativeComponent* itemSelector READ itemSelector WRITE setItemSelector NOTIFY itemSelectorChanged)
    Q_PROPERTY(QWebPreferences* preferences READ preferences CONSTANT FINAL)
    Q_PROPERTY(bool useTraditionalDesktopBehaviour READ useTraditionalDesktopBehaviour WRITE setUseTraditionalDesktopBehaviour)
    Q_PROPERTY(QWebViewportInfo* viewportInfo READ viewportInfo CONSTANT FINAL)
    Q_PROPERTY(QDeclarativeListProperty<QQuickUrlSchemeDelegate> urlSchemeDelegates READ schemeDelegates)
    Q_ENUMS(NavigationRequestAction)

public:
    enum NavigationRequestAction {
        DownloadRequest = 2
    };

    QQuickWebViewExperimental(QQuickWebView* webView);
    virtual ~QQuickWebViewExperimental();

    QDeclarativeComponent* alertDialog() const;
    void setAlertDialog(QDeclarativeComponent*);
    QDeclarativeComponent* confirmDialog() const;
    void setConfirmDialog(QDeclarativeComponent*);
    QDeclarativeComponent* promptDialog() const;
    void setPromptDialog(QDeclarativeComponent*);
    QDeclarativeComponent* authenticationDialog() const;
    void setAuthenticationDialog(QDeclarativeComponent*);
    QDeclarativeComponent* certificateVerificationDialog() const;
    void setCertificateVerificationDialog(QDeclarativeComponent*);
    QDeclarativeComponent* itemSelector() const;
    void setItemSelector(QDeclarativeComponent*);
    
    QWebViewportInfo* viewportInfo();

    QWebPreferences* preferences() const;
    bool useTraditionalDesktopBehaviour() const;
    QWebNavigationHistory* navigationHistory() const;
    QQuickWebPage* page();

    static QQuickUrlSchemeDelegate* schemeDelegates_At(QDeclarativeListProperty<QQuickUrlSchemeDelegate>*, int index);
    static void schemeDelegates_Append(QDeclarativeListProperty<QQuickUrlSchemeDelegate>*, QQuickUrlSchemeDelegate*);
    static int schemeDelegates_Count(QDeclarativeListProperty<QQuickUrlSchemeDelegate>*);
    static void schemeDelegates_Clear(QDeclarativeListProperty<QQuickUrlSchemeDelegate>*);
    QDeclarativeListProperty<QQuickUrlSchemeDelegate> schemeDelegates();
    void invokeApplicationSchemeHandler(WTF::PassRefPtr<WebKit::QtRefCountedNetworkRequestData>);
    void sendApplicationSchemeReply(QQuickNetworkReply*);

public Q_SLOTS:
    void setUseTraditionalDesktopBehaviour(bool enable);
    void goBackTo(int index);
    void goForwardTo(int index);
    void postMessage(const QString&);

Q_SIGNALS:
    void alertDialogChanged();
    void confirmDialogChanged();
    void promptDialogChanged();
    void authenticationDialogChanged();
    void certificateVerificationDialogChanged();
    void itemSelectorChanged();
    void downloadRequested(QWebDownloadItem* downloadItem);
    void permissionRequested(QWebPermissionRequest* permission);
    void messageReceived(const QVariantMap& message);

private:
    QQuickWebView* q_ptr;
    QQuickWebViewPrivate* d_ptr;
    QObject* schemeParent;
    QWebViewportInfo* m_viewportInfo;

    friend class QtWebPageUIClient;

    Q_DECLARE_PRIVATE(QQuickWebView)
    Q_DECLARE_PUBLIC(QQuickWebView)
};

#endif // qquickwebview_p_h
