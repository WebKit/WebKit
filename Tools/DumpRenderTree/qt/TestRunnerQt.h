/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef TestRunnerQt_h
#define TestRunnerQt_h

#include <QBasicTimer>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QTimerEvent>
#include <QVariant>
#include <QtDebug>

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

class TestRunnerQt : public QObject {
    Q_OBJECT
    Q_PROPERTY(int webHistoryItemCount READ webHistoryItemCount)
    Q_PROPERTY(bool globalFlag READ globalFlag WRITE setGlobalFlag)
public:
    TestRunnerQt(WebCore::DumpRenderTree*);

    bool shouldDisallowIncreaseForApplicationCacheQuota() const { return m_disallowIncreaseForApplicationCacheQuota; }
    bool shouldDumpAsText() const { return m_textDump; }
    bool shouldDumpAsAudio() const { return m_audioDump; }
    bool shouldDumpPixels() const { return m_shouldDumpPixels; }
    bool shouldDumpBackForwardList() const { return m_dumpBackForwardList; }
    bool shouldDumpChildrenAsText() const { return m_dumpChildrenAsText; }
    bool shouldDumpChildFrameScrollPositions() const { return m_dumpChildFrameScrollPositions; }
    bool shouldDumpDatabaseCallbacks() const { return m_dumpDatabaseCallbacks; }
    bool shouldDumpApplicationCacheDelegateCallbacks() const { return m_dumpApplicationCacheDelegateCallbacks; }
    bool shouldDumpStatusCallbacks() const { return m_dumpStatusCallbacks; }
    bool shouldWaitUntilDone() const { return m_waitForDone; }
    bool shouldHandleErrorPages() const { return m_handleErrorPages; }
    bool canOpenWindows() const { return m_canOpenWindows; }
    bool shouldDumpTitleChanges() const { return m_dumpTitleChanges; }
    bool waitForPolicy() const { return m_waitForPolicy; }
    bool ignoreReqestForPermission() const { return m_ignoreDesktopNotification; }
    bool isPrinting() { return m_isPrinting; }

    const QByteArray& audioData() const { return m_audioData; }

    void reset();

    static const unsigned int maxViewWidth;
    static const unsigned int maxViewHeight;

    void setTimeout(int timeout) { m_timeout = timeout; }
    void setShouldTimeout(bool flag) { m_shouldTimeout = flag; }

protected:
    void timerEvent(QTimerEvent*);

Q_SIGNALS:
    void done();

    void showPage();
    void hidePage();
    void geolocationPermissionSet();

public Q_SLOTS:
    void maybeDump(bool ok);
    void disallowIncreaseForApplicationCacheQuota() { m_disallowIncreaseForApplicationCacheQuota = true; }
    void dumpAsText(bool shouldDumpPixels = false);
    void dumpChildFramesAsText() { m_dumpChildrenAsText = true; }
    void dumpChildFrameScrollPositions() { m_dumpChildFrameScrollPositions = true; }
    void dumpDatabaseCallbacks() { m_dumpDatabaseCallbacks = true; }
    void dumpApplicationCacheDelegateCallbacks() { m_dumpApplicationCacheDelegateCallbacks = true; }
    void dumpStatusCallbacks() { m_dumpStatusCallbacks = true; }
    void dumpNotifications();
    void setCanOpenWindows() { m_canOpenWindows = true; }
    void setPrinting() { m_isPrinting = true; }
    void waitUntilDone();
    int webHistoryItemCount();
    void keepWebHistory();
    void notifyDone();
    void dumpBackForwardList() { m_dumpBackForwardList = true; }
    bool globalFlag() const { return m_globalFlag; }
    void setGlobalFlag(bool flag) { m_globalFlag = flag; }
    void handleErrorPages() { m_handleErrorPages = true; }
    void dumpEditingCallbacks();
    void dumpFrameLoadCallbacks();
    void dumpProgressFinishedCallback();
    void dumpUserGestureInFrameLoadCallbacks();
    void dumpResourceLoadCallbacks();
    void dumpResourceResponseMIMETypes();
    void dumpWillCacheResponse();
    void dumpHistoryCallbacks();
    void setWillSendRequestReturnsNullOnRedirect(bool enabled);
    void setWillSendRequestReturnsNull(bool enabled);
    void setWillSendRequestClearHeader(const QStringList& headers);
    void queueBackNavigation(int howFarBackward);
    void queueForwardNavigation(int howFarForward);
    void queueLoad(const QString& url, const QString& target = QString());
    void queueLoadHTMLString(const QString& content, const QString& baseURL = QString(), const QString& failingURL = QString());
    void queueReload();
    void queueLoadingScript(const QString& script);
    void queueNonLoadingScript(const QString& script);
    void provisionalLoad();
    void setCloseRemainingWindowsWhenComplete(bool = false) { }
    int windowCount();
    void ignoreLegacyWebNotificationPermissionRequests();
    void simulateLegacyWebNotificationClick(const QString& title);
    void grantWebNotificationPermission(const QString& origin);
    void denyWebNotificationPermission(const QString& origin);
    void removeAllWebNotificationPermissions();
    void simulateWebNotificationClick(const QWebElement&);
    void display();
    void displayInvalidatedRegion();
    void clearBackForwardList();
    QString pathToLocalResource(const QString& url);
    void dumpTitleChanges() { m_dumpTitleChanges = true; }
    QString encodeHostName(const QString& host);
    QString decodeHostName(const QString& host);
    void dumpSelectionRect() const { }
    void setDeveloperExtrasEnabled(bool);
    void setAsynchronousSpellCheckingEnabled(bool);
    void showWebInspector();
    void closeWebInspector();
    void evaluateInWebInspector(long callId, const QString& script);
    void removeAllVisitedLinks();
    void setAllowUniversalAccessFromFileURLs(bool enable);
    void setAllowFileAccessFromFileURLs(bool enable);
    void setAppCacheMaximumSize(unsigned long long quota);
    void setValueForUser(const QWebElement&, const QString& value);
    void setFixedContentsSize(int width, int height);
    void setPrivateBrowsingEnabled(bool);
    void setSpatialNavigationEnabled(bool);
    void setPluginsEnabled(bool flag);
    void setPopupBlockingEnabled(bool);
    void setPOSIXLocale(const QString& locale);
    void resetLoadFinished() { m_loadFinished = false; }
    void setWindowIsKey(bool);
    void setMainFrameIsFirstResponder(bool);
    void setDeferMainResourceDataLoad(bool);
    void setJavaScriptCanAccessClipboard(bool enable);
    void setXSSAuditorEnabled(bool);
    void setCaretBrowsingEnabled(bool);
    void setAuthorAndUserStylesEnabled(bool);
    void setViewModeMediaFeature(const QString& mode);
    void setSmartInsertDeleteEnabled(bool);
    void setSelectTrailingWhitespaceEnabled(bool);
    void execCommand(const QString& name, const QString& value = QString());
    bool isCommandEnabled(const QString& name) const;
    bool findString(const QString&, const QStringList& optionArray);

    bool elementDoesAutoCompleteForElementWithId(const QString& elementId);

    void addOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);

    void dispatchPendingLoadRequests();

    void clearAllApplicationCaches();
    void clearApplicationCacheForOrigin(const QString& url);
    void setApplicationCacheOriginQuota(unsigned long long);
    QStringList originsWithApplicationCache();
    long long applicationCacheDiskUsageForOrigin(const QString&); 
    void setCacheModel(int);

    void setDatabaseQuota(int size);
    void clearAllDatabases();
    void setIconDatabaseEnabled(bool);

    void setCustomPolicyDelegate(bool enabled, bool permissive = false);
    void waitForPolicyDelegate();

    void overridePreference(const QString& name, const QVariant& value);
    void setUserStyleSheetLocation(const QString& url);
    void setUserStyleSheetEnabled(bool);
    void setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme);
    bool callShouldCloseOnWebView();
    // For now, this is a no-op. This may change depending on outcome of
    // https://bugs.webkit.org/show_bug.cgi?id=33333
    void setCallCloseOnWebViews() { }
    // This is a no-op - it allows us to pass
    // plugins/get-url-that-the-resource-load-delegate-will-disallow.html
    // which is a Mac-specific test.
    void addDisallowedURL(const QString&) { }

    void setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma);

    void setMockGeolocationPositionUnavailableError(const QString& message);
    void setMockGeolocationPosition(double latitude, double longitude, double accuracy);
    void setGeolocationPermission(bool allow);
    int numberOfPendingGeolocationPermissionRequests();
    bool isGeolocationPermissionSet() const { return m_isGeolocationPermissionSet; }
    bool geolocationPermission() const { return m_geolocationPermission; }

    void addMockSpeechInputResult(const QString& result, double confidence, const QString& language);
    void setMockSpeechInputDumpRect(bool flag);
    void startSpeechInput(const QString& inputElement);

    void setPageVisibility(const char*);
    void resetPageVisibility();

    void setAutomaticLinkDetectionEnabled(bool);

    // Empty stub method to keep parity with object model exposed by global TestRunner.
    void abortModal() { }

    void addURLToRedirect(const QString& origin, const QString& destination);

    /*
        Policy values: 'on', 'auto' or 'off'.
        Orientation values: 'vertical' or 'horizontal'.
    */
    void setScrollbarPolicy(const QString& orientation, const QString& policy);

    QString markerTextForListItem(const QWebElement& listItem);
    QVariantMap computedStyleIncludingVisitedInfo(const QWebElement&) const;

    // Simulate a request an embedding application could make, populating per-session credential storage.
    void authenticateSession(const QString& url, const QString& username, const QString& password);

    void evaluateScriptInIsolatedWorldAndReturnValue(int worldID, const QString& script);
    void evaluateScriptInIsolatedWorld(int worldID, const QString& script);
    void addUserStyleSheet(const QString& sourceCode);
    
    void originsWithLocalStorage();
    void deleteAllLocalStorage();
    void deleteLocalStorageForOrigin(const QString& originIdentifier);
    long long localStorageDiskUsageForOrigin(const QString& originIdentifier);
    void observeStorageTrackerNotifications(unsigned number);
    void syncLocalStorage();
    void setTextDirection(const QString& directionName);
    void goBack();
    void setDefersLoading(bool);
    void setAlwaysAcceptCookies(bool);
    void setAlwaysBlockCookies(bool);

    void setAudioData(const QByteArray&);

private Q_SLOTS:
    void processWork();

private:
    void setGeolocationPermissionCommon(bool allow);

private:
    bool m_hasDumped;
    bool m_textDump;
    bool m_audioDump;
    bool m_shouldDumpPixels;
    bool m_disallowIncreaseForApplicationCacheQuota;
    bool m_dumpBackForwardList;
    bool m_dumpChildrenAsText;
    bool m_dumpChildFrameScrollPositions;
    bool m_canOpenWindows;
    bool m_waitForDone;
    bool m_dumpTitleChanges;
    bool m_dumpDatabaseCallbacks;
    bool m_dumpApplicationCacheDelegateCallbacks;
    bool m_dumpStatusCallbacks;
    bool m_waitForPolicy;
    bool m_handleErrorPages;
    bool m_loadFinished;
    bool m_globalFlag;
    bool m_userStyleSheetEnabled;
    bool m_isGeolocationPermissionSet;
    bool m_isPrinting;
    bool m_geolocationPermission;

    QUrl m_userStyleSheetLocation;
    QBasicTimer m_timeoutTimer;
    QWebFrame* m_topLoadingFrame;
    WebCore::DumpRenderTree* m_drt;
    QWebHistory* m_webHistory;
    bool m_ignoreDesktopNotification;

    QByteArray m_audioData;

    bool m_shouldTimeout;
    int m_timeout;
};

#endif // TestRunnerQt_h
