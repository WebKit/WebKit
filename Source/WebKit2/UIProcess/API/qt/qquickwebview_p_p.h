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

#ifndef qquickwebview_p_p_h
#define qquickwebview_p_p_h

#include "QtPageClient.h"
#include "QtWebPageEventHandler.h"
#include "QtViewportInteractionEngine.h"
#include "QtWebPageLoadClient.h"
#include "QtWebPagePolicyClient.h"
#include "QtWebPageProxy.h"
#include "QtWebPageUIClient.h"

#include "qquickwebview_p.h"
#include "qquickwebpage_p.h"

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <wtf/OwnPtr.h>

class QtWebPageProxy;

QT_BEGIN_NAMESPACE
class QDeclarativeComponent;
class QFileDialog;
QT_END_NAMESPACE

class QQuickWebViewPrivate {
    Q_DECLARE_PUBLIC(QQuickWebView)
    friend class QQuickWebViewExperimental;

public:
    static QQuickWebViewPrivate* get(QQuickWebView* q) { return q->d_ptr.data(); }

    QQuickWebViewPrivate(QQuickWebView* viewport, WKContextRef contextRef = 0, WKPageGroupRef pageGroupRef = 0);
    virtual ~QQuickWebViewPrivate(); 
    void setPageProxy(QtWebPageProxy*);
    void initializeTouch(QQuickWebView* viewport);
    void initializeDesktop(QQuickWebView* viewport);
    void enableMouseEvents();
    void disableMouseEvents();

    void loadDidCommit();

    void didFinishFirstNonEmptyLayout();
    void didChangeContentsSize(const QSize& newSize);
    void didChangeViewportProperties(const WebCore::ViewportArguments& args);
    void didChangeBackForwardList();

    void scrollPositionRequested(const QPoint& pos);
    void updateViewportSize();
    QtViewportInteractionEngine::Constraints computeViewportConstraints();

    void updateVisibleContentRect();

    void _q_suspend();
    void _q_resume();
    void _q_viewportTrajectoryVectorChanged(const QPointF&);
    void _q_onOpenPanelFilesSelected();
    void _q_onOpenPanelFinished(int result);
    void _q_onVisibleChanged();

    void chooseFiles(WKOpenPanelResultListenerRef, const QStringList& selectedFileNames, QtWebPageUIClient::FileChooserType);
    void runJavaScriptAlert(const QString&);
    bool runJavaScriptConfirm(const QString&);
    QString runJavaScriptPrompt(const QString&, const QString& defaultValue, bool& ok);

    void setUseTraditionalDesktopBehaviour(bool enable);
    void setViewInAttachedProperties(QObject*);

private:
    // This class is responsible for collecting and applying all properties
    // on the viewport item, when transitioning from page A to page B is finished.
    // See more at https://trac.webkit.org/wiki/QtWebKitLayoutInteraction
    class PostTransitionState {
    public:
        PostTransitionState(QQuickWebViewPrivate* parent)
            : p(parent)
        { }

        void apply()
        {
            p->interactionEngine->reset();
            p->interactionEngine->applyConstraints(p->computeViewportConstraints());
            p->interactionEngine->pagePositionRequest(position);

            if (contentsSize.isValid()) {
                p->pageView->setWidth(contentsSize.width());
                p->pageView->setHeight(contentsSize.height());
            }

            position = QPoint();
            contentsSize = QSize();
        }

        QQuickWebViewPrivate* p;
        QSize contentsSize;
        QPoint position;
    };

    QScopedPointer<QtPageClient> pageClient;
    QScopedPointer<QtWebPageEventHandler> eventHandler;

    QScopedPointer<QtWebPageLoadClient> pageLoadClient;
    QScopedPointer<QtWebPagePolicyClient> pagePolicyClient;
    QScopedPointer<QtWebPageUIClient> pageUIClient;

    QScopedPointer<QQuickWebPage> pageView;
    QScopedPointer<QtViewportInteractionEngine> interactionEngine;

    QQuickWebView* q_ptr;
    QScopedPointer<QtWebPageProxy> pageProxy;

    QDeclarativeComponent* alertDialog;
    QDeclarativeComponent* confirmDialog;
    QDeclarativeComponent* promptDialog;
    QDeclarativeComponent* itemSelector;

    WebCore::ViewportArguments viewportArguments;
    OwnPtr<PostTransitionState> postTransitionState;
    bool isTransitioningToNewPage;
    bool pageIsSuspended;

    bool useTraditionalDesktopBehaviour;
    QFileDialog* fileDialog;
    WKOpenPanelResultListenerRef openPanelResultListener;
};

#endif // qquickwebview_p_p_h
