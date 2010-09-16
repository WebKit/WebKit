/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutTestControllerQt_h
#define LayoutTestControllerQt_h

#include <QBasicTimer>
#include <QObject>
#include <QSize>
#include <QString>
#include <QtDebug>
#include <QTimer>
#include <QTimerEvent>
#include <QVariant>

#include <qwebdatabase.h>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qwebhistory.h>
#include <qwebpage.h>
#include <qwebsecurityorigin.h>

class QWebFrame;
class DumpRenderTreeSupportQt;
namespace WebCore {
    class DumpRenderTree;
}
class LayoutTestController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int webHistoryItemCount READ webHistoryItemCount)
    Q_PROPERTY(int workerThreadCount READ workerThreadCount)
    Q_PROPERTY(bool globalFlag READ globalFlag WRITE setGlobalFlag)
public:
    LayoutTestController(WebCore::DumpRenderTree* drt);

    bool shouldDumpAsText() const { return m_textDump; }
    bool shouldDumpBackForwardList() const { return m_dumpBackForwardList; }
    bool shouldDumpChildrenAsText() const { return m_dumpChildrenAsText; }
    bool shouldDumpChildFrameScrollPositions() const { return m_dumpChildFrameScrollPositions; }
    bool shouldDumpDatabaseCallbacks() const { return m_dumpDatabaseCallbacks; }
    bool shouldDumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    bool shouldWaitUntilDone() const { return m_waitForDone; }
    bool shouldHandleErrorPages() const { return m_handleErrorPages; }
    bool canOpenWindows() const { return m_canOpenWindows; }
    bool shouldDumpTitleChanges() const { return m_dumpTitleChanges; }
    bool waitForPolicy() const { return m_waitForPolicy; }
    bool ignoreReqestForPermission() const { return m_ignoreDesktopNotification; }

    void reset();

    static const unsigned int maxViewWidth;
    static const unsigned int maxViewHeight;

protected:
    void timerEvent(QTimerEvent*);

signals:
    void done();

    void showPage();
    void hidePage();
    void geolocationPermissionSet();

public slots:
    void maybeDump(bool ok);
    void dumpAsText() { m_textDump = true; }
    void dumpChildFramesAsText() { m_dumpChildrenAsText = true; }
    void dumpChildFrameScrollPositions() { m_dumpChildFrameScrollPositions = true; }
    void dumpDatabaseCallbacks() { m_dumpDatabaseCallbacks = true; }
    void dumpStatusCallbacks() { m_dumpStatusCallbacks = true; }
    void setCanOpenWindows() { m_canOpenWindows = true; }
    void waitUntilDone();
    QString counterValueForElementById(const QString& id);
    int webHistoryItemCount();
    void keepWebHistory();
    void notifyDone();
    void dumpBackForwardList() { m_dumpBackForwardList = true; }
    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool flag) { m_globalFlag = flag; }
    void handleErrorPages() { m_handleErrorPages = true; }
    void dumpEditingCallbacks();
    void dumpFrameLoadCallbacks();
    void dumpResourceLoadCallbacks();
    void dumpResourceResponseMIMETypes();
    void dumpHistoryCallbacks();
    void dumpConfigurationForViewport(int availableWidth, int availableHeight);
    void setWillSendRequestReturnsNullOnRedirect(bool enabled);
    void setWillSendRequestReturnsNull(bool enabled);
    void setWillSendRequestClearHeader(const QStringList& headers);
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(const QString& url, const QString& target = QString());
    void queueLoadHTMLString(const QString& content, const QString& baseURL = QString());
    void queueReload();
    void queueLoadingScript(const QString& script);
    void queueNonLoadingScript(const QString& script);
    void provisionalLoad();
    void setCloseRemainingWindowsWhenComplete(bool = false) {}
    int windowCount();
    void grantDesktopNotificationPermission(const QString& origin);
    void ignoreDesktopNotificationPermissionRequests();
    bool checkDesktopNotificationPermission(const QString& origin);
    void simulateDesktopNotificationClick(const QString& title);
    void display();
    void clearBackForwardList();
    QString pathToLocalResource(const QString& url);
    void dumpTitleChanges() { m_dumpTitleChanges = true; }
    QString encodeHostName(const QString& host);
    QString decodeHostName(const QString& host);
    void dumpSelectionRect() const {}
    void setDeveloperExtrasEnabled(bool);
    void showWebInspector();
    void closeWebInspector();
    void evaluateInWebInspector(long callId, const QString& script);
    void removeAllVisitedLinks();

    void setMediaType(const QString& type);
    void setFrameFlatteningEnabled(bool enable);
    void setAllowUniversalAccessFromFileURLs(bool enable);
    void setAllowFileAccessFromFileURLs(bool enable);
    void setAppCacheMaximumSize(unsigned long long quota);
    void setJavaScriptProfilingEnabled(bool enable);
    void setTimelineProfilingEnabled(bool enable);
    void setFixedContentsSize(int width, int height);
    void setPrivateBrowsingEnabled(bool enable);
    void setSpatialNavigationEnabled(bool enabled);
    void setPluginsEnabled(bool flag);
    void setPopupBlockingEnabled(bool enable);
    void setPOSIXLocale(const QString& locale);
    void resetLoadFinished() { m_loadFinished = false; }
    void setWindowIsKey(bool isKey);
    void setMainFrameIsFirstResponder(bool isFirst);
    void setDeferMainResourceDataLoad(bool);
    void setJavaScriptCanAccessClipboard(bool enable);
    void setXSSAuditorEnabled(bool enable);
    void setCaretBrowsingEnabled(bool enable);
    void setViewModeMediaFeature(const QString& mode);
    void setSmartInsertDeleteEnabled(bool enable);
    void setSelectTrailingWhitespaceEnabled(bool enable);
    void execCommand(const QString& name, const QString& value = QString());
    bool isCommandEnabled(const QString& name) const;

    bool pauseAnimationAtTimeOnElementWithId(const QString& animationName, double time, const QString& elementId);
    bool pauseTransitionAtTimeOnElementWithId(const QString& propertyName, double time, const QString& elementId);
    bool sampleSVGAnimationForElementAtTime(const QString& animationId, double time, const QString& elementId);
    bool elementDoesAutoCompleteForElementWithId(const QString& elementId);

    unsigned numberOfActiveAnimations() const;
    void suspendAnimations() const;
    void resumeAnimations() const;

    void addOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);

    void dispatchPendingLoadRequests();
    void disableImageLoading();

    void clearAllApplicationCaches();
    void setApplicationCacheOriginQuota(unsigned long long quota);

    void setDatabaseQuota(int size);
    void clearAllDatabases();
    void setIconDatabaseEnabled(bool enable);

    void setCustomPolicyDelegate(bool enabled, bool permissive = true);
    void waitForPolicyDelegate();

    void overridePreference(const QString& name, const QVariant& value);
    void setUserStyleSheetLocation(const QString& url);
    void setUserStyleSheetEnabled(bool enabled);
    void setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme);
    int workerThreadCount();
    int pageNumberForElementById(const QString& id, float width = 0, float height = 0);
    int numberOfPages(float width = maxViewWidth, float height = maxViewHeight);
    bool callShouldCloseOnWebView();
    // For now, this is a no-op. This may change depending on outcome of
    // https://bugs.webkit.org/show_bug.cgi?id=33333
    void setCallCloseOnWebViews() {}
    // This is a no-op - it allows us to pass
    // plugins/get-url-that-the-resource-load-delegate-will-disallow.html
    // which is a Mac-specific test.
    void addDisallowedURL(const QString&) {}

    void setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma);

    void setMockGeolocationError(int code, const QString& message);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy);
    void setGeolocationPermission(bool allow);
    bool isGeolocationPermissionSet() const { return m_isGeolocationPermissionSet; }
    bool geolocationPermission() const { return m_geolocationPermission; }

    void setMockSpeechInputResult(const QString& result);

    // Empty stub method to keep parity with object model exposed by global LayoutTestController.
    void abortModal() {}

    /*
        Policy values: 'on', 'auto' or 'off'.
        Orientation values: 'vertical' or 'horizontal'.
    */
    void setScrollbarPolicy(const QString& orientation, const QString& policy);

    QString markerTextForListItem(const QWebElement& listItem);
    QVariantMap computedStyleIncludingVisitedInfo(const QWebElement& element) const;

    // Simulate a request an embedding application could make, populating per-session credential storage.
    void authenticateSession(const QString& url, const QString& username, const QString& password);

    void setEditingBehavior(const QString& editingBehavior);

    void evaluateScriptInIsolatedWorld(int worldID, const QString& script);
    bool isPageBoxVisible(int pageIndex);
    QString pageSizeAndMarginsInPixels(int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft);
    QString pageProperty(const QString& propertyName, int pageNumber);
    void addUserStyleSheet(const QString& sourceCode);

private slots:
    void processWork();

private:
    void setGeolocationPermissionCommon(bool allow);

private:
    bool m_hasDumped;
    bool m_textDump;
    bool m_dumpBackForwardList;
    bool m_dumpChildrenAsText;
    bool m_dumpChildFrameScrollPositions;
    bool m_canOpenWindows;
    bool m_waitForDone;
    bool m_dumpTitleChanges;
    bool m_dumpDatabaseCallbacks;
    bool m_dumpStatusCallbacks;
    bool m_waitForPolicy;
    bool m_handleErrorPages;
    bool m_loadFinished;
    bool m_globalFlag;
    bool m_userStyleSheetEnabled;
    bool m_isGeolocationPermissionSet;
    bool m_geolocationPermission;

    QUrl m_userStyleSheetLocation;
    QBasicTimer m_timeoutTimer;
    QWebFrame* m_topLoadingFrame;
    WebCore::DumpRenderTree* m_drt;
    QWebHistory* m_webHistory;
    QStringList m_desktopNotificationAllowedOrigins;
    bool m_ignoreDesktopNotification;
};

#endif // LayoutTestControllerQt_h
