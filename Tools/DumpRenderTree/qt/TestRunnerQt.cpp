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
#include "config.h"
#include "TestRunnerQt.h"

#include "DumpRenderTreeQt.h"
#include "DumpRenderTreeSupportQt.h"
#include "WorkQueue.h"
#include "WorkQueueItemQt.h"
#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <qwebsettings.h>

TestRunner::TestRunner(WebCore::DumpRenderTree* drt)
    : QObject()
    , m_drt(drt)
    , m_shouldTimeout(true)
    , m_timeout(30000)
{
    reset();
    DumpRenderTreeSupportQt::dumpNotification(true);
}

void TestRunner::reset()
{
    m_hasDumped = false;
    m_loadFinished = false;
    m_textDump = false;
    m_dumpBackForwardList = false;
    m_dumpChildrenAsText = false;
    m_dumpChildFrameScrollPositions = false;
    m_canOpenWindows = false;
    m_waitForDone = false;
    m_disallowIncreaseForApplicationCacheQuota = false;
    m_dumpTitleChanges = false;
    m_dumpDatabaseCallbacks = false;
    m_dumpApplicationCacheDelegateCallbacks = false;
    m_dumpStatusCallbacks = false;
    m_timeoutTimer.stop();
    m_topLoadingFrame = 0;
    m_waitForPolicy = false;
    m_handleErrorPages = false;
    m_webHistory = 0;
    m_globalFlag = false;
    m_userStyleSheetEnabled = false;
    m_desktopNotificationAllowedOrigins.clear();
    m_ignoreDesktopNotification = false;
    m_isGeolocationPermissionSet = false;
    m_isPrinting = false;
    m_geolocationPermission = false;

    DumpRenderTreeSupportQt::dumpEditingCallbacks(false);
    DumpRenderTreeSupportQt::dumpFrameLoader(false);
    DumpRenderTreeSupportQt::dumpProgressFinishedCallback(false);
    DumpRenderTreeSupportQt::dumpUserGestureInFrameLoader(false);
    DumpRenderTreeSupportQt::dumpResourceLoadCallbacks(false);
    DumpRenderTreeSupportQt::dumpResourceResponseMIMETypes(false);
    DumpRenderTreeSupportQt::dumpWillCacheResponseCallbacks(false);
    DumpRenderTreeSupportQt::setDeferMainResourceDataLoad(true);
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNullOnRedirect(false);
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNull(false);
    DumpRenderTreeSupportQt::setWillSendRequestClearHeaders(QStringList());
    DumpRenderTreeSupportQt::clearScriptWorlds();
    DumpRenderTreeSupportQt::setCustomPolicyDelegate(false, false);
    DumpRenderTreeSupportQt::dumpHistoryCallbacks(false);
    DumpRenderTreeSupportQt::dumpVisitedLinksCallbacks(false);
    DumpRenderTreeSupportQt::resetGeolocationMock(m_drt->webPage());
    setIconDatabaseEnabled(false);
    clearAllDatabases();
    // The default state for DRT is to block third-party cookies, mimicing the Mac port
    setAlwaysAcceptCookies(false);
    emit hidePage();
}

void TestRunner::processWork()
{
    // qDebug() << ">>>processWork";

    // if we didn't start a new load, then we finished all the commands, so we're ready to dump state
    if (WorkQueue::shared()->processWork() && !shouldWaitUntilDone()) {
        emit done();
        m_hasDumped = true;
    }
}

// Called on loadFinished on WebPage
void TestRunner::maybeDump(bool /*success*/)
{

    // This can happen on any of the http/tests/security/window-events-*.html tests, where the test opens
    // a new window, calls the unload and load event handlers on the window's page, and then immediately
    // issues a notifyDone. Needs investigation.
    if (!m_topLoadingFrame)
        return;

    // It is possible that we get called by windows created from the main page that have finished
    // loading, so we don't ASSERT here. At the moment we do not gather results from such windows,
    // but may need to in future.
    if (sender() != m_topLoadingFrame->page())
        return;

    m_loadFinished = true;
    // as the function is called on loadFinished, the test might
    // already have dumped and thus no longer be active, thus
    // bail out here.
    if (m_hasDumped)
        return;

    WorkQueue::shared()->setFrozen(true); // first complete load freezes the queue for the rest of this test
    if (WorkQueue::shared()->count())
        QTimer::singleShot(0, this, SLOT(processWork()));
    else if (!shouldWaitUntilDone()) {
        emit done();
        m_hasDumped = true;
    }
}

void TestRunner::waitUntilDone()
{
    //qDebug() << ">>>>waitForDone";
    m_waitForDone = true;

    if (!m_shouldTimeout)
        return;

    m_timeoutTimer.start(m_timeout, this);
}

void TestRunner::setViewModeMediaFeature(const QString& mode)
{
    m_drt->webPage()->setProperty("_q_viewMode", mode);
}

int TestRunner::webHistoryItemCount()
{
    if (!m_webHistory)
        return -1;

    // Subtract one here as our QWebHistory::count() includes the actual page,
    // which is not considered in the DRT tests.
    return m_webHistory->count() - 1;
}

void TestRunner::keepWebHistory()
{
    m_webHistory = m_drt->webPage()->history();
}

void TestRunner::notifyDone()
{
    qDebug() << ">>>>notifyDone";

    if (m_shouldTimeout && !m_timeoutTimer.isActive())
        return;

    m_timeoutTimer.stop();
    m_waitForDone = false;

    // If the page has not finished loading (i.e. loadFinished() has not been emitted) then
    // content created by the likes of document.write() JS methods will not be available yet.
    // When the page has finished loading, maybeDump above will dump the results now that we have
    // just set shouldWaitUntilDone to false.
    if (!m_loadFinished)
        return;

    emit done();

    // FIXME: investigate why always resetting these result in timeouts
    m_hasDumped = true;
    m_waitForPolicy = false;
}

int TestRunner::windowCount()
{
    return m_drt->windowCount();
}

void TestRunner::grantDesktopNotificationPermission(const QString& origin)
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    m_drt->webPage()->setFeaturePermission(frame, QWebPage::Notifications, QWebPage::PermissionGrantedByUser);
    m_desktopNotificationAllowedOrigins.append(origin);
}

void TestRunner::ignoreDesktopNotificationPermissionRequests()
{
    m_ignoreDesktopNotification = true;
}

bool TestRunner::checkDesktopNotificationPermission(const QString& origin)
{
    return !m_ignoreDesktopNotification && m_desktopNotificationAllowedOrigins.contains(origin);
}

void TestRunner::simulateDesktopNotificationClick(const QString& title)
{
    DumpRenderTreeSupportQt::simulateDesktopNotificationClick(title);
}

void TestRunner::display()
{
    emit showPage();
}

void TestRunner::displayInvalidatedRegion()
{
    display();
}

void TestRunner::clearBackForwardList()
{
    m_drt->webPage()->history()->clear();
}

QString TestRunner::pathToLocalResource(const QString& url)
{
    QString localTmpUrl(QLatin1String("file:///tmp/LayoutTests"));

    // Translate a request for /tmp/LayoutTests to the repository LayoutTests directory.
    // Do not rely on a symlink to be created via the test runner, which will not work on Windows.
    if (url.startsWith(localTmpUrl)) {
        // DumpRenderTree lives in WebKit/WebKitBuild/<build_mode>/bin.
        // Translate from WebKit/WebKitBuild/Release/bin => WebKit/LayoutTests.
        QFileInfo layoutTestsRoot(QCoreApplication::applicationDirPath() + QLatin1String("/../../../LayoutTests/"));
        if (layoutTestsRoot.exists())
            return QLatin1String("file://") + layoutTestsRoot.absolutePath() + url.mid(localTmpUrl.length());
    }

    return url;
}

void TestRunner::dumpEditingCallbacks()
{
    qDebug() << ">>>dumpEditingCallbacks";
    DumpRenderTreeSupportQt::dumpEditingCallbacks(true);
}

void TestRunner::dumpFrameLoadCallbacks()
{
    DumpRenderTreeSupportQt::dumpFrameLoader(true);
}

void TestRunner::dumpProgressFinishedCallback()
{
    DumpRenderTreeSupportQt::dumpProgressFinishedCallback(true);
}

void TestRunner::dumpUserGestureInFrameLoadCallbacks()
{
    DumpRenderTreeSupportQt::dumpUserGestureInFrameLoader(true);
}

void TestRunner::dumpResourceLoadCallbacks()
{
    DumpRenderTreeSupportQt::dumpResourceLoadCallbacks(true);
}

void TestRunner::dumpResourceResponseMIMETypes()
{
    DumpRenderTreeSupportQt::dumpResourceResponseMIMETypes(true);
}

void TestRunner::dumpWillCacheResponse()
{
    DumpRenderTreeSupportQt::dumpWillCacheResponseCallbacks(true);
}

void TestRunner::dumpHistoryCallbacks()
{
    DumpRenderTreeSupportQt::dumpHistoryCallbacks(true);
}

void TestRunner::setWillSendRequestReturnsNullOnRedirect(bool enabled)
{
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNullOnRedirect(enabled);
}

void TestRunner::setWillSendRequestReturnsNull(bool enabled)
{
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNull(enabled);
}

void TestRunner::setWillSendRequestClearHeader(const QStringList& headers)
{
    DumpRenderTreeSupportQt::setWillSendRequestClearHeaders(headers);
}

void TestRunner::setDeferMainResourceDataLoad(bool defer)
{
    DumpRenderTreeSupportQt::setDeferMainResourceDataLoad(defer);
}

void TestRunner::queueBackNavigation(int howFarBackward)
{
    //qDebug() << ">>>queueBackNavigation" << howFarBackward;
    for (int i = 0; i != howFarBackward; ++i)
        WorkQueue::shared()->queue(new BackItem(1, m_drt->webPage()));
}

void TestRunner::queueForwardNavigation(int howFarForward)
{
    //qDebug() << ">>>queueForwardNavigation" << howFarForward;
    for (int i = 0; i != howFarForward; ++i)
        WorkQueue::shared()->queue(new ForwardItem(1, m_drt->webPage()));
}

void TestRunner::queueLoad(const QString& url, const QString& target)
{
    //qDebug() << ">>>queueLoad" << url << target;
    QUrl mainResourceUrl = m_drt->webPage()->mainFrame()->url();
    QString absoluteUrl = mainResourceUrl.resolved(QUrl(url)).toEncoded();
    WorkQueue::shared()->queue(new LoadItem(absoluteUrl, target, m_drt->webPage()));
}

void TestRunner::queueLoadHTMLString(const QString& content, const QString& baseURL, const QString& failingURL)
{
    if (failingURL.isEmpty())
        WorkQueue::shared()->queue(new LoadHTMLStringItem(content, baseURL, m_drt->webPage()));
    else
        WorkQueue::shared()->queue(new LoadAlternateHTMLStringItem(content, baseURL, failingURL, m_drt->webPage()));
}

void TestRunner::queueReload()
{
    //qDebug() << ">>>queueReload";
    WorkQueue::shared()->queue(new ReloadItem(m_drt->webPage()));
}

void TestRunner::queueLoadingScript(const QString& script)
{
    //qDebug() << ">>>queueLoadingScript" << script;
    WorkQueue::shared()->queue(new LoadingScriptItem(script, m_drt->webPage()));
}

void TestRunner::queueNonLoadingScript(const QString& script)
{
    //qDebug() << ">>>queueNonLoadingScript" << script;
    WorkQueue::shared()->queue(new NonLoadingScriptItem(script, m_drt->webPage()));
}

void TestRunner::provisionalLoad()
{
    QWebFrame* frame = qobject_cast<QWebFrame*>(sender());
    if (!m_topLoadingFrame && !m_hasDumped)
        m_topLoadingFrame = frame;
}

void TestRunner::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == m_timeoutTimer.timerId()) {
        const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
        fprintf(stderr, "%s", message);
        fprintf(stdout, "%s", message);
        notifyDone();
    } else
        QObject::timerEvent(ev);
}

QString TestRunner::encodeHostName(const QString& host)
{
    QString encoded = QString::fromLatin1(QUrl::toAce(host + QLatin1String(".no")));
    encoded.truncate(encoded.length() - 3); // strip .no
    return encoded;
}

QString TestRunner::decodeHostName(const QString& host)
{
    QString decoded = QUrl::fromAce(host.toLatin1() + QByteArray(".no"));
    decoded.truncate(decoded.length() - 3);
    return decoded;
}

void TestRunner::setMediaType(const QString& type)
{
    DumpRenderTreeSupportQt::setMediaType(m_drt->webPage()->mainFrame(), type);
}

void TestRunner::closeWebInspector()
{
    DumpRenderTreeSupportQt::webInspectorClose(m_drt->webPage());
    m_drt->webPage()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, false);
}

void TestRunner::setDeveloperExtrasEnabled(bool enabled)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, enabled);
}

void TestRunner::setAsynchronousSpellCheckingEnabled(bool)
{
    // FIXME: Implement this.
}

void TestRunner::showWebInspector()
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    DumpRenderTreeSupportQt::webInspectorShow(m_drt->webPage());
}

void TestRunner::evaluateInWebInspector(long callId, const QString& script)
{
    DumpRenderTreeSupportQt::webInspectorExecuteScript(m_drt->webPage(), callId, script);
}

void TestRunner::setFrameFlatteningEnabled(bool enabled)
{
    DumpRenderTreeSupportQt::setFrameFlatteningEnabled(m_drt->webPage(), enabled);
}

void TestRunner::setMockScrollbarsEnabled(bool enabled)
{
    DumpRenderTreeSupportQt::setMockScrollbarsEnabled(m_drt->webPage(), enabled);
}

void TestRunner::goBack()
{
    DumpRenderTreeSupportQt::goBack(m_drt->webPage());
}

void TestRunner::setDefersLoading(bool flag)
{
    DumpRenderTreeSupportQt::setDefersLoading(m_drt->webPage(), flag);
}

void TestRunner::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::LocalContentCanAccessRemoteUrls, enabled);
}

void TestRunner::setAllowFileAccessFromFileURLs(bool enabled)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::LocalContentCanAccessFileUrls, enabled);
}

void TestRunner::setAppCacheMaximumSize(unsigned long long quota)
{
    m_drt->webPage()->settings()->setOfflineWebApplicationCacheQuota(quota);
}

void TestRunner::setAutofilled(const QWebElement& element, bool isAutofilled)
{
    return DumpRenderTreeSupportQt::setAutofilled(element, isAutofilled);
}

void TestRunner::setValueForUser(const QWebElement& element, const QString& value)
{
    DumpRenderTreeSupportQt::setValueForUser(element, value);
}

void TestRunner::setFixedContentsSize(int width, int height)
{
    m_topLoadingFrame->page()->setPreferredContentsSize(QSize(width, height));
}

void TestRunner::setPrivateBrowsingEnabled(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, enable);
}

void TestRunner::setSpatialNavigationEnabled(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::SpatialNavigationEnabled, enable);
}

void TestRunner::setPopupBlockingEnabled(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, !enable);
}

void TestRunner::setPluginsEnabled(bool flag)
{
    // FIXME: Implement
}

void TestRunner::setPOSIXLocale(const QString& locale)
{
    QLocale qlocale(locale);
    QLocale::setDefault(qlocale);
} 

void TestRunner::setWindowIsKey(bool isKey)
{
    m_drt->switchFocus(isKey);
}

void TestRunner::setMainFrameIsFirstResponder(bool isFirst)
{
    //FIXME: only need this for the moment: https://bugs.webkit.org/show_bug.cgi?id=32990
}

void TestRunner::setJavaScriptCanAccessClipboard(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::JavascriptCanAccessClipboard, enable);
}

void TestRunner::setXSSAuditorEnabled(bool enable)
{
    // Set XSSAuditingEnabled globally so that windows created by the test inherit it too.
    // resetSettings() will call this to reset the page and global setting to false again.
    // Needed by http/tests/security/xssAuditor/link-opens-new-window.html
    QWebSettings* globalSettings = QWebSettings::globalSettings();
    globalSettings->setAttribute(QWebSettings::XSSAuditingEnabled, enable);
    m_drt->webPage()->settings()->setAttribute(QWebSettings::XSSAuditingEnabled, enable);
}

bool TestRunner::pauseAnimationAtTimeOnElementWithId(const QString& animationName,
                                                               double time,
                                                               const QString& elementId)
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::pauseAnimation(frame, animationName, time, elementId);
}

bool TestRunner::pauseTransitionAtTimeOnElementWithId(const QString& propertyName,
                                                                double time,
                                                                const QString& elementId)
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::pauseTransitionOfProperty(frame, propertyName, time, elementId);
}

unsigned TestRunner::numberOfActiveAnimations() const
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::numberOfActiveAnimations(frame);
}

void TestRunner::disableImageLoading()
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::AutoLoadImages, false);
}

void TestRunner::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
}

void TestRunner::clearAllApplicationCaches()
{
    DumpRenderTreeSupportQt::clearAllApplicationCaches();
}

void TestRunner::clearApplicationCacheForOrigin(const QString& url)
{
    // FIXME: Implement to support deleting all application caches for an origin.
}

long long TestRunner::localStorageDiskUsageForOrigin(const QString& originIdentifier)
{
    // FIXME: Implement to support getting disk usage in bytes for an origin.
    return 0;
}

void TestRunner::setApplicationCacheOriginQuota(unsigned long long quota)
{
    if (!m_topLoadingFrame)
        return;
    m_topLoadingFrame->securityOrigin().setApplicationCacheQuota(quota);
}

long long TestRunner::applicationCacheDiskUsageForOrigin(const QString& origin)
{
    // FIXME: Implement to support getting disk usage by all application caches for an origin.
    return 0;
}

QStringList TestRunner::originsWithApplicationCache()
{
    // FIXME: Implement to get origins that have application caches.
    return QStringList();
}

void TestRunner::setCacheModel(int model)
{
    // qwebsetting doesn't have matched setting yet :
    // WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER
    // WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER
    // WEBKIT_CACHE_MODEL_WEB_BROWSER

    // FIXME: Implement.
}

void TestRunner::setDatabaseQuota(int size)
{
    if (!m_topLoadingFrame)
        return;
    m_topLoadingFrame->securityOrigin().setDatabaseQuota(size);
}

void TestRunner::clearAllDatabases()
{
    QWebDatabase::removeAllDatabases();
}

void TestRunner::addOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains)
{
    DumpRenderTreeSupportQt::whiteListAccessFromOrigin(sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void TestRunner::removeOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains)
{
    DumpRenderTreeSupportQt::removeWhiteListAccessFromOrigin(sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void TestRunner::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    DumpRenderTreeSupportQt::setCustomPolicyDelegate(enabled, permissive);
}

void TestRunner::waitForPolicyDelegate()
{
    setCustomPolicyDelegate(true);
    m_waitForPolicy = true;
    waitUntilDone();
}

void TestRunner::overridePreference(const QString& name, const QVariant& value)
{
    QWebSettings* settings = m_topLoadingFrame->page()->settings();

    if (name == "WebKitJavaScriptEnabled")
        settings->setAttribute(QWebSettings::JavascriptEnabled, value.toBool());
    else if (name == "WebKitTabToLinksPreferenceKey")
        settings->setAttribute(QWebSettings::LinksIncludedInFocusChain, value.toBool());
    else if (name == "WebKitOfflineWebApplicationCacheEnabled")
        settings->setAttribute(QWebSettings::OfflineWebApplicationCacheEnabled, value.toBool());
    else if (name == "WebKitDefaultFontSize")
        settings->setFontSize(QWebSettings::DefaultFontSize, value.toInt());
    else if (name == "WebKitUsesPageCachePreferenceKey")
        QWebSettings::setMaximumPagesInCache(value.toInt());
    else if (name == "WebKitEnableCaretBrowsing")
        setCaretBrowsingEnabled(value.toBool());
    else if (name == "WebKitPluginsEnabled")
        settings->setAttribute(QWebSettings::PluginsEnabled, value.toBool());
    else if (name == "WebKitWebGLEnabled")
        settings->setAttribute(QWebSettings::WebGLEnabled, value.toBool());
    else if (name == "WebKitCSSRegionsEnabled")
        settings->setAttribute(QWebSettings::CSSRegionsEnabled, value.toBool());
    else if (name == "WebKitCSSGridLayoutEnabled")
        settings->setAttribute(QWebSettings::CSSGridLayoutEnabled, value.toBool());
    else if (name == "WebKitHyperlinkAuditingEnabled")
        settings->setAttribute(QWebSettings::HyperlinkAuditingEnabled, value.toBool());
    else if (name == "WebKitAcceleratedCompositingEnabled")
        settings->setAttribute(QWebSettings::AcceleratedCompositingEnabled, value.toBool());
    else
        printf("ERROR: TestRunner::overridePreference() does not support the '%s' preference\n",
            name.toLatin1().data());
}

void TestRunner::setUserStyleSheetLocation(const QString& url)
{
    QByteArray urlData = pathToLocalResource(url).toLatin1();
    m_userStyleSheetLocation = QUrl::fromEncoded(urlData, QUrl::StrictMode);

    if (m_userStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void TestRunner::setCaretBrowsingEnabled(bool value)
{
    DumpRenderTreeSupportQt::setCaretBrowsingEnabled(m_drt->webPage(), value);
}

void TestRunner::setAuthorAndUserStylesEnabled(bool value)
{
    DumpRenderTreeSupportQt::setAuthorAndUserStylesEnabled(m_drt->webPage(), value);
}

void TestRunner::setUserStyleSheetEnabled(bool enabled)
{
    m_userStyleSheetEnabled = enabled;

    if (enabled)
        m_drt->webPage()->settings()->setUserStyleSheetUrl(m_userStyleSheetLocation);
    else
        m_drt->webPage()->settings()->setUserStyleSheetUrl(QUrl());
}

void TestRunner::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme)
{
    DumpRenderTreeSupportQt::setDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

int TestRunner::workerThreadCount()
{
    return DumpRenderTreeSupportQt::workerThreadCount();
}

int TestRunner::numberOfPages(float width, float height)
{
    return DumpRenderTreeSupportQt::numberOfPages(m_drt->webPage()->mainFrame(), width, height);
}

bool TestRunner::callShouldCloseOnWebView()
{
    return DumpRenderTreeSupportQt::shouldClose(m_drt->webPage()->mainFrame());
}

void TestRunner::setScrollbarPolicy(const QString& orientation, const QString& policy)
{
    Qt::Orientation o;
    Qt::ScrollBarPolicy p;

    if (orientation == "vertical")
        o = Qt::Vertical;
    else if (orientation == "horizontal")
        o = Qt::Horizontal;
    else
        return;

    if (policy == "on")
        p = Qt::ScrollBarAlwaysOn;
    else if (policy == "auto")
        p = Qt::ScrollBarAsNeeded;
    else if (policy == "off")
        p = Qt::ScrollBarAlwaysOff;
    else
        return;

    m_drt->webPage()->mainFrame()->setScrollBarPolicy(o, p);
}

void TestRunner::setSmartInsertDeleteEnabled(bool enable)
{
    DumpRenderTreeSupportQt::setSmartInsertDeleteEnabled(m_drt->webPage(), enable);
}

void TestRunner::setSelectTrailingWhitespaceEnabled(bool enable)
{
    DumpRenderTreeSupportQt::setSelectTrailingWhitespaceEnabled(m_drt->webPage(), enable);
}

void TestRunner::execCommand(const QString& name, const QString& value)
{
    DumpRenderTreeSupportQt::executeCoreCommandByName(m_drt->webPage(), name, value);
}

bool TestRunner::isCommandEnabled(const QString& name) const
{
    return DumpRenderTreeSupportQt::isCommandEnabled(m_drt->webPage(), name);
}

bool TestRunner::findString(const QString& string, const QStringList& optionArray)
{
    return DumpRenderTreeSupportQt::findString(m_drt->webPage(), string, optionArray);
}

QString TestRunner::markerTextForListItem(const QWebElement& listItem)
{
    return DumpRenderTreeSupportQt::markerTextForListItem(listItem);
}

QVariantMap TestRunner::computedStyleIncludingVisitedInfo(const QWebElement& element) const
{
    return DumpRenderTreeSupportQt::computedStyleIncludingVisitedInfo(element);
}

bool TestRunner::elementDoesAutoCompleteForElementWithId(const QString& elementId)
{
    return DumpRenderTreeSupportQt::elementDoesAutoCompleteForElementWithId(m_drt->webPage()->mainFrame(), elementId);
}

void TestRunner::authenticateSession(const QString&, const QString&, const QString&)
{
    // FIXME: If there is a concept per-session (per-process) credential storage, the credentials should be added to it for later use.
}

void TestRunner::setIconDatabaseEnabled(bool enable)
{
    if (enable && !m_drt->persistentStoragePath().isEmpty())
        QWebSettings::setIconDatabasePath(m_drt->persistentStoragePath());
    else
        QWebSettings::setIconDatabasePath(QString());
}

void TestRunner::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    QList<WebCore::WebPage*> pages = m_drt->getAllPages();
    foreach (WebCore::WebPage* page, pages)
        DumpRenderTreeSupportQt::setMockDeviceOrientation(page, canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);
}

void TestRunner::setGeolocationPermission(bool allow)
{
    setGeolocationPermissionCommon(allow);
    QList<WebCore::WebPage*> pages = m_drt->getAllPages();
    foreach (WebCore::WebPage* page, pages)
        DumpRenderTreeSupportQt::setMockGeolocationPermission(page, allow);
}

int TestRunner::numberOfPendingGeolocationPermissionRequests()
{
    int pendingPermissionCount = 0;
    QList<WebCore::WebPage*> pages = m_drt->getAllPages();
    foreach (WebCore::WebPage* page, pages)
        pendingPermissionCount += DumpRenderTreeSupportQt::numberOfPendingGeolocationPermissionRequests(page);

    return pendingPermissionCount;
}

void TestRunner::setGeolocationPermissionCommon(bool allow)
{
     m_isGeolocationPermissionSet = true;
     m_geolocationPermission = allow;
}

void TestRunner::setMockGeolocationError(int code, const QString& message)
{
    QList<WebCore::WebPage*> pages = m_drt->getAllPages();
    foreach (WebCore::WebPage* page, pages)
        DumpRenderTreeSupportQt::setMockGeolocationError(page, code, message);
}

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    QList<WebCore::WebPage*> pages = m_drt->getAllPages();
    foreach (WebCore::WebPage* page, pages)
        DumpRenderTreeSupportQt::setMockGeolocationPosition(page, latitude, longitude, accuracy);
}

void TestRunner::addMockSpeechInputResult(const QString& result, double confidence, const QString& language)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void TestRunner::setMockSpeechInputDumpRect(bool flag)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void TestRunner::startSpeechInput(const QString& inputElement)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void TestRunner::evaluateScriptInIsolatedWorldAndReturnValue(int worldID, const QString& script)
{
    // FIXME: Implement.
}

void TestRunner::evaluateScriptInIsolatedWorld(int worldID, const QString& script)
{
    DumpRenderTreeSupportQt::evaluateScriptInIsolatedWorld(m_drt->webPage()->mainFrame(), worldID, script);
}

QString TestRunner::pageSizeAndMarginsInPixels(int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    return DumpRenderTreeSupportQt::pageSizeAndMarginsInPixels(m_drt->webPage()->mainFrame(), pageIndex,
                                                               width, height, marginTop, marginRight, marginBottom, marginLeft);
}

QString TestRunner::pageProperty(const QString& propertyName, int pageNumber)
{
    return DumpRenderTreeSupportQt::pageProperty(m_drt->webPage()->mainFrame(), propertyName, pageNumber);
}

void TestRunner::addUserStyleSheet(const QString& sourceCode)
{
    DumpRenderTreeSupportQt::addUserStyleSheet(m_drt->webPage(), sourceCode);
}

void TestRunner::removeAllVisitedLinks()
{
    QWebHistory* history = m_drt->webPage()->history();
    history->clear();
    DumpRenderTreeSupportQt::dumpVisitedLinksCallbacks(true);
}

void TestRunner::addURLToRedirect(const QString& origin, const QString& destination)
{
    DumpRenderTreeSupportQt::addURLToRedirect(origin, destination);
}

void TestRunner::setMinimumTimerInterval(double minimumTimerInterval)
{
    DumpRenderTreeSupportQt::setMinimumTimerInterval(m_drt->webPage(), minimumTimerInterval);
}

void TestRunner::originsWithLocalStorage()
{
    // FIXME: Implement.
}

void TestRunner::deleteAllLocalStorage()
{
    // FIXME: Implement.
}

void TestRunner::deleteLocalStorageForOrigin(const QString& originIdentifier)
{
    // FIXME: Implement.
}

void TestRunner::observeStorageTrackerNotifications(unsigned number)
{
    // FIXME: Implement.
}

void TestRunner::syncLocalStorage()
{
    // FIXME: Implement.
}

void TestRunner::resetPageVisibility()
{
    // FIXME: Implement this.
}

void TestRunner::setPageVisibility(const char*)
{
    // FIXME: Implement this.
}

void TestRunner::setAutomaticLinkDetectionEnabled(bool)
{
    // FIXME: Implement this.
}

QString TestRunner::layerTreeAsText()
{
    return DumpRenderTreeSupportQt::layerTreeAsText(m_drt->webPage()->mainFrame());
}

void TestRunner::setTextDirection(const QString& directionName)
{
    if (directionName == "auto")
        m_drt->webPage()->triggerAction(QWebPage::SetTextDirectionDefault);
    else if (directionName == "rtl")
        m_drt->webPage()->triggerAction(QWebPage::SetTextDirectionRightToLeft);
    else if (directionName == "ltr")
        m_drt->webPage()->triggerAction(QWebPage::SetTextDirectionLeftToRight);
}

void TestRunner::setAlwaysAcceptCookies(bool accept)
{
    QWebSettings* globalSettings = QWebSettings::globalSettings();
    if (accept)
        globalSettings->setThirdPartyCookiePolicy(QWebSettings::AlwaysAllowThirdPartyCookies);
    else {
        // This matches the Safari third-party cookie blocking policy tested in third-party-cookie-relaxing.html
        globalSettings->setThirdPartyCookiePolicy(QWebSettings::AllowThirdPartyWithExistingCookies);
    }
}

void TestRunner::setAlwaysBlockCookies(bool block)
{
    QWebSettings* globalSettings = QWebSettings::globalSettings();
    if (block)
        globalSettings->setThirdPartyCookiePolicy(QWebSettings::AlwaysBlockThirdPartyCookies);
    else
        globalSettings->setThirdPartyCookiePolicy(QWebSettings::AlwaysAllowThirdPartyCookies);
}

const unsigned TestRunner::maxViewWidth = 800;
const unsigned TestRunner::maxViewHeight = 600;
