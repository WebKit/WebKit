/*
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef QWebPageAdapter_h
#define QWebPageAdapter_h

#include "QWebPageClient.h"
#include "ViewportArguments.h"

#include <qnetworkrequest.h>
#include <qrect.h>
#include <qscopedpointer.h>
#include <qsharedpointer.h>
#include <qstring.h>
#include <qurl.h>

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QNetworkAccessManager;
QT_END_NAMESPACE

namespace WebCore {
class Page;
class ChromeClientQt;
class GeolocationClientQt;
class UndoStep;
}

class QtPluginWidgetAdapter;
class QWebFrameAdapter;
class QWebHistoryItem;
class QWebPageClient;
class QWebPluginFactory;
class QWebSecurityOrigin;
class QWebSelectMethod;
class QWebSettings;
class QWebFullScreenVideoHandler;
class UndoStepQt;


class QWebPageAdapter {
public:
    QWebPageAdapter();
    virtual ~QWebPageAdapter();

    void init(WebCore::Page*);
    // Called manually from ~QWebPage destructor to ensure that
    // the QWebPageAdapter and the QWebPagePrivate are intact when
    // various destruction callbacks from WebCore::Page::~Page() hit us.
    void deletePage();

    virtual void show() = 0;
    virtual void setFocus() = 0;
    virtual void unfocus() = 0;
    virtual void setWindowRect(const QRect&) = 0;
    virtual QSize viewportSize() const = 0;
    virtual QWebPageAdapter* createWindow(bool /*dialog*/) = 0;
    virtual QObject* handle() = 0;
    virtual void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID) = 0;
    virtual void javaScriptAlert(QWebFrameAdapter*, const QString& msg) = 0;
    virtual bool javaScriptConfirm(QWebFrameAdapter*, const QString& msg) = 0;
    virtual bool javaScriptPrompt(QWebFrameAdapter*, const QString& msg, const QString& defaultValue, QString* result) = 0;
    virtual bool shouldInterruptJavaScript() = 0;
    virtual void printRequested(QWebFrameAdapter*) = 0;
    virtual void databaseQuotaExceeded(QWebFrameAdapter*, const QString& databaseName) = 0;
    virtual void applicationCacheQuotaExceeded(QWebSecurityOrigin*, quint64 defaultOriginQuota, quint64 totalSpaceNeeded) = 0;
    virtual void setToolTip(const QString&) = 0;
    virtual QStringList chooseFiles(QWebFrameAdapter*, bool allowMultiple, const QStringList& suggestedFileNames) = 0;
    virtual QColor colorSelectionRequested(const QColor& selectedColor) = 0;
    virtual QWebSelectMethod* createSelectPopup() = 0;
    virtual QRect viewRectRelativeToWindow() = 0;

#if USE(QT_MULTIMEDIA)
    virtual QWebFullScreenVideoHandler* createFullScreenVideoHandler() = 0;
#endif
    virtual void geolocationPermissionRequested(QWebFrameAdapter*) = 0;
    virtual void geolocationPermissionRequestCancelled(QWebFrameAdapter*) = 0;
    virtual void notificationsPermissionRequested(QWebFrameAdapter*) = 0;
    virtual void notificationsPermissionRequestCancelled(QWebFrameAdapter*) = 0;

    virtual void respondToChangedContents() = 0;
    virtual void respondToChangedSelection() = 0;
    virtual void microFocusChanged() = 0;
    virtual void triggerCopyAction() = 0;
    virtual void triggerActionForKeyEvent(QKeyEvent*) = 0;
    virtual void clearUndoStack() = 0;
    virtual bool canUndo() const = 0;
    virtual bool canRedo() const = 0;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual const char* editorCommandForKeyEvent(QKeyEvent*) = 0;
    virtual void createUndoStep(QSharedPointer<UndoStepQt>) = 0;

    virtual void updateNavigationActions() = 0;

    virtual QWebFrameAdapter* mainFrameAdapter() = 0;

    virtual QObject* inspectorHandle() = 0;
    virtual void setInspectorFrontend(QObject*) = 0;
    virtual void setInspectorWindowTitle(const QString&) = 0;
    virtual void createWebInspector(QObject** inspectorView, QWebPageAdapter** inspectorPage) = 0;
    virtual QStringList menuActionsAsText() = 0;
    virtual void emitViewportChangeRequested() = 0;
    virtual bool acceptNavigationRequest(QWebFrameAdapter*, const QNetworkRequest&, int type) = 0;
    virtual void emitRestoreFrameStateRequested(QWebFrameAdapter *) = 0;
    virtual void emitSaveFrameStateRequested(QWebFrameAdapter *, QWebHistoryItem*) = 0;
    virtual void emitDownloadRequested(const QNetworkRequest&) = 0;
    virtual void emitFrameCreated(QWebFrameAdapter*) = 0;
    virtual QString userAgentForUrl(const QUrl&) const = 0;
    virtual bool supportsErrorPageExtension() const = 0;
    struct ErrorPageOption {
        QUrl url;
        QWebFrameAdapter* frame;
        QString domain;
        int error;
        QString errorString;
    };
    struct ErrorPageReturn {
        QString contentType;
        QString encoding;
        QUrl baseUrl;
        QByteArray content;
    };
    virtual bool errorPageExtension(ErrorPageOption*, ErrorPageReturn*) = 0;
    virtual QtPluginWidgetAdapter* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&) = 0;
    virtual QtPluginWidgetAdapter* adapterForWidget(QObject*) const = 0;


    static QWebPageAdapter* kit(WebCore::Page*);
    WebCore::ViewportArguments viewportArguments();
    void registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep>);

    void setNetworkAccessManager(QNetworkAccessManager*);
    QNetworkAccessManager* networkAccessManager();

    QWebSettings *settings;

    WebCore::Page *page;
    QScopedPointer<QWebPageClient> client;

    QWebPluginFactory *pluginFactory;

    bool forwardUnsupportedContent;
    bool insideOpenCall;

private:
    QNetworkAccessManager *networkManager;

public:
    static bool drtRun;

    friend class WebCore::ChromeClientQt;
    friend class WebCore::GeolocationClientQt;
};

#endif // QWebPageAdapter_h
