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
#include "LayoutTestControllerQt.h"
#include "../../../WebKit/qt/WebCoreSupport/DumpRenderTreeSupportQt.h"

#include "DumpRenderTreeQt.h"
#include "WorkQueue.h"
#include "WorkQueueItemQt.h"
#include <QDir>
#include <QLocale>
#include <qwebsettings.h>

LayoutTestController::LayoutTestController(WebCore::DumpRenderTree* drt)
    : QObject()
    , m_drt(drt)
{
    qRegisterMetaType<QWebElement>("QWebElement");
    reset();
    DumpRenderTreeSupportQt::dumpNotification(true);
}

void LayoutTestController::reset()
{
    m_hasDumped = false;
    m_loadFinished = false;
    m_textDump = false;
    m_dumpBackForwardList = false;
    m_dumpChildrenAsText = false;
    m_dumpChildFrameScrollPositions = false;
    m_canOpenWindows = false;
    m_waitForDone = false;
    m_dumpTitleChanges = false;
    m_dumpDatabaseCallbacks = false;
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
    m_geolocationPermission = false;

    DumpRenderTreeSupportQt::dumpEditingCallbacks(false);
    DumpRenderTreeSupportQt::dumpFrameLoader(false);
    DumpRenderTreeSupportQt::dumpResourceLoadCallbacks(false);
    DumpRenderTreeSupportQt::dumpResourceResponseMIMETypes(false);
    DumpRenderTreeSupportQt::setDeferMainResourceDataLoad(true);
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNullOnRedirect(false);
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNull(false);
    DumpRenderTreeSupportQt::setWillSendRequestClearHeaders(QStringList());
    DumpRenderTreeSupportQt::clearScriptWorlds();
    DumpRenderTreeSupportQt::setCustomPolicyDelegate(false, false);
    DumpRenderTreeSupportQt::dumpHistoryCallbacks(false);
    DumpRenderTreeSupportQt::dumpVisitedLinksCallbacks(false);
    setIconDatabaseEnabled(false);

    emit hidePage();
}

void LayoutTestController::processWork()
{
    // qDebug() << ">>>processWork";

    // if we didn't start a new load, then we finished all the commands, so we're ready to dump state
    if (WorkQueue::shared()->processWork() && !shouldWaitUntilDone()) {
        emit done();
        m_hasDumped = true;
    }
}

// Called on loadFinished on WebPage
void LayoutTestController::maybeDump(bool success)
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
        if (success)
            emit done();
        m_hasDumped = true;
    }
}

void LayoutTestController::waitUntilDone()
{
    //qDebug() << ">>>>waitForDone";
    m_waitForDone = true;
    m_timeoutTimer.start(30000, this);
}

QString LayoutTestController::counterValueForElementById(const QString& id)
{
    return DumpRenderTreeSupportQt::counterValueForElementById(m_drt->webPage()->mainFrame(), id);
}

void LayoutTestController::setViewModeMediaFeature(const QString& mode)
{
    m_drt->webPage()->setProperty("_q_viewMode", mode);
}

int LayoutTestController::webHistoryItemCount()
{
    if (!m_webHistory)
        return -1;

    // Subtract one here as our QWebHistory::count() includes the actual page,
    // which is not considered in the DRT tests.
    return m_webHistory->count() - 1;
}

void LayoutTestController::keepWebHistory()
{
    m_webHistory = m_drt->webPage()->history();
}

void LayoutTestController::notifyDone()
{
    qDebug() << ">>>>notifyDone";

    if (!m_timeoutTimer.isActive())
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

int LayoutTestController::windowCount()
{
    return m_drt->windowCount();
}

void LayoutTestController::grantDesktopNotificationPermission(const QString& origin)
{
    m_desktopNotificationAllowedOrigins.append(origin);
}

void LayoutTestController::ignoreDesktopNotificationPermissionRequests()
{
    m_ignoreDesktopNotification = true;
}

bool LayoutTestController::checkDesktopNotificationPermission(const QString& origin)
{
    return !m_ignoreDesktopNotification && m_desktopNotificationAllowedOrigins.contains(origin);
}

void LayoutTestController::simulateDesktopNotificationClick(const QString& title)
{
    DumpRenderTreeSupportQt::simulateDesktopNotificationClick(title);
}

void LayoutTestController::display()
{
    emit showPage();
}

void LayoutTestController::clearBackForwardList()
{
    m_drt->webPage()->history()->clear();
}

QString LayoutTestController::pathToLocalResource(const QString& url)
{
    // Function introduced in r28690.
    return QDir::toNativeSeparators(url);
}

void LayoutTestController::dumpConfigurationForViewport(int availableWidth, int availableHeight)
{
    QString res = DumpRenderTreeSupportQt::viewportAsText(m_drt->webPage(), QSize(availableWidth, availableHeight));
    fputs(qPrintable(res), stdout);
}

void LayoutTestController::dumpEditingCallbacks()
{
    qDebug() << ">>>dumpEditingCallbacks";
    DumpRenderTreeSupportQt::dumpEditingCallbacks(true);
}

void LayoutTestController::dumpFrameLoadCallbacks()
{
    DumpRenderTreeSupportQt::dumpFrameLoader(true);
}

void LayoutTestController::dumpResourceLoadCallbacks()
{
    DumpRenderTreeSupportQt::dumpResourceLoadCallbacks(true);
}

void LayoutTestController::dumpResourceResponseMIMETypes()
{
    DumpRenderTreeSupportQt::dumpResourceResponseMIMETypes(true);
}

void LayoutTestController::dumpHistoryCallbacks()
{
    DumpRenderTreeSupportQt::dumpHistoryCallbacks(true);
}

void LayoutTestController::setWillSendRequestReturnsNullOnRedirect(bool enabled)
{
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNullOnRedirect(enabled);
}

void LayoutTestController::setWillSendRequestReturnsNull(bool enabled)
{
    DumpRenderTreeSupportQt::setWillSendRequestReturnsNull(enabled);
}

void LayoutTestController::setWillSendRequestClearHeader(const QStringList& headers)
{
    DumpRenderTreeSupportQt::setWillSendRequestClearHeaders(headers);
}

void LayoutTestController::setDeferMainResourceDataLoad(bool defer)
{
    DumpRenderTreeSupportQt::setDeferMainResourceDataLoad(defer);
}

void LayoutTestController::queueBackNavigation(int howFarBackward)
{
    //qDebug() << ">>>queueBackNavigation" << howFarBackward;
    WorkQueue::shared()->queue(new BackItem(howFarBackward, m_drt->webPage()));
}

void LayoutTestController::queueForwardNavigation(int howFarForward)
{
    //qDebug() << ">>>queueForwardNavigation" << howFarForward;
    WorkQueue::shared()->queue(new ForwardItem(howFarForward, m_drt->webPage()));
}

void LayoutTestController::queueLoad(const QString& url, const QString& target)
{
    //qDebug() << ">>>queueLoad" << url << target;
    QUrl mainResourceUrl = m_drt->webPage()->mainFrame()->url();
    QString absoluteUrl = mainResourceUrl.resolved(QUrl(url)).toEncoded();
    WorkQueue::shared()->queue(new LoadItem(absoluteUrl, target, m_drt->webPage()));
}

void LayoutTestController::queueLoadHTMLString(const QString& content, const QString& baseURL)
{
    WorkQueue::shared()->queue(new LoadHTMLStringItem(content, baseURL, m_drt->webPage()));
}

void LayoutTestController::queueReload()
{
    //qDebug() << ">>>queueReload";
    WorkQueue::shared()->queue(new ReloadItem(m_drt->webPage()));
}

void LayoutTestController::queueLoadingScript(const QString& script)
{
    //qDebug() << ">>>queueLoadingScript" << script;
    WorkQueue::shared()->queue(new LoadingScriptItem(script, m_drt->webPage()));
}

void LayoutTestController::queueNonLoadingScript(const QString& script)
{
    //qDebug() << ">>>queueNonLoadingScript" << script;
    WorkQueue::shared()->queue(new NonLoadingScriptItem(script, m_drt->webPage()));
}

void LayoutTestController::provisionalLoad()
{
    QWebFrame* frame = qobject_cast<QWebFrame*>(sender());
    if (!m_topLoadingFrame && !m_hasDumped)
        m_topLoadingFrame = frame;
}

void LayoutTestController::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == m_timeoutTimer.timerId()) {
        const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
        fprintf(stderr, "%s", message);
        fprintf(stdout, "%s", message);
        notifyDone();
    } else
        QObject::timerEvent(ev);
}

QString LayoutTestController::encodeHostName(const QString& host)
{
    QString encoded = QString::fromLatin1(QUrl::toAce(host + QLatin1String(".no")));
    encoded.truncate(encoded.length() - 3); // strip .no
    return encoded;
}

QString LayoutTestController::decodeHostName(const QString& host)
{
    QString decoded = QUrl::fromAce(host.toLatin1() + QByteArray(".no"));
    decoded.truncate(decoded.length() - 3);
    return decoded;
}

void LayoutTestController::setMediaType(const QString& type)
{
    DumpRenderTreeSupportQt::setMediaType(m_drt->webPage()->mainFrame(), type);
}

void LayoutTestController::closeWebInspector()
{
    DumpRenderTreeSupportQt::webInspectorClose(m_drt->webPage());
    m_drt->webPage()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, false);
}

void LayoutTestController::setDeveloperExtrasEnabled(bool enabled)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, enabled);
}

void LayoutTestController::showWebInspector()
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    DumpRenderTreeSupportQt::webInspectorShow(m_drt->webPage());
}

void LayoutTestController::evaluateInWebInspector(long callId, const QString& script)
{
    DumpRenderTreeSupportQt::webInspectorExecuteScript(m_drt->webPage(), callId, script);
}

void LayoutTestController::setFrameFlatteningEnabled(bool enabled)
{
    DumpRenderTreeSupportQt::setFrameFlatteningEnabled(m_drt->webPage(), enabled);
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::LocalContentCanAccessRemoteUrls, enabled);
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool enabled)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::LocalContentCanAccessFileUrls, enabled);
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long quota)
{
    m_drt->webPage()->settings()->setOfflineWebApplicationCacheQuota(quota);
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool enable)
{
    setDeveloperExtrasEnabled(enable);
    DumpRenderTreeSupportQt::setJavaScriptProfilingEnabled(m_topLoadingFrame, enable);
}

void LayoutTestController::setTimelineProfilingEnabled(bool enable)
{
    DumpRenderTreeSupportQt::setTimelineProfilingEnabled(m_drt->webPage(), enable);
}

void LayoutTestController::setFixedContentsSize(int width, int height)
{
    m_topLoadingFrame->page()->setPreferredContentsSize(QSize(width, height));
}

void LayoutTestController::setPrivateBrowsingEnabled(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, enable);
}

void LayoutTestController::setSpatialNavigationEnabled(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::SpatialNavigationEnabled, enable);
}

void LayoutTestController::setPopupBlockingEnabled(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, !enable);
}

void LayoutTestController::setPluginsEnabled(bool flag)
{
    // FIXME: Implement
}

void LayoutTestController::setPOSIXLocale(const QString& locale)
{
    QLocale qlocale(locale);
    QLocale::setDefault(qlocale);
} 

void LayoutTestController::setWindowIsKey(bool isKey)
{
    m_drt->switchFocus(isKey);
}

void LayoutTestController::setMainFrameIsFirstResponder(bool isFirst)
{
    //FIXME: only need this for the moment: https://bugs.webkit.org/show_bug.cgi?id=32990
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool enable)
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::JavascriptCanAccessClipboard, enable);
}

void LayoutTestController::setXSSAuditorEnabled(bool enable)
{
    // Set XSSAuditingEnabled globally so that windows created by the test inherit it too.
    // resetSettings() will call this to reset the page and global setting to false again.
    // Needed by http/tests/security/xssAuditor/link-opens-new-window.html
    QWebSettings* globalSettings = QWebSettings::globalSettings();
    globalSettings->setAttribute(QWebSettings::XSSAuditingEnabled, enable);
    m_drt->webPage()->settings()->setAttribute(QWebSettings::XSSAuditingEnabled, enable);
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(const QString& animationName,
                                                               double time,
                                                               const QString& elementId)
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::pauseAnimation(frame, animationName, time, elementId);
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(const QString& propertyName,
                                                                double time,
                                                                const QString& elementId)
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::pauseTransitionOfProperty(frame, propertyName, time, elementId);
}

bool LayoutTestController::sampleSVGAnimationForElementAtTime(const QString& animationId,
                                                              double time,
                                                              const QString& elementId)
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::pauseSVGAnimation(frame, animationId, time, elementId);
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    return DumpRenderTreeSupportQt::numberOfActiveAnimations(frame);
}

void LayoutTestController::suspendAnimations() const
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    DumpRenderTreeSupportQt::suspendAnimations(frame);
}

void LayoutTestController::resumeAnimations() const
{
    QWebFrame* frame = m_drt->webPage()->mainFrame();
    Q_ASSERT(frame);
    DumpRenderTreeSupportQt::resumeAnimations(frame);
}

void LayoutTestController::disableImageLoading()
{
    m_drt->webPage()->settings()->setAttribute(QWebSettings::AutoLoadImages, false);
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
}

void LayoutTestController::clearAllApplicationCaches()
{
    // FIXME: implement to support Application Cache quotas.
}

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long quota)
{
    // FIXME: implement to support Application Cache quotas.
}

void LayoutTestController::setDatabaseQuota(int size)
{
    if (!m_topLoadingFrame)
        return;
    m_topLoadingFrame->securityOrigin().setDatabaseQuota(size);
}

void LayoutTestController::clearAllDatabases()
{
    QWebDatabase::removeAllDatabases();
}

void LayoutTestController::addOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains)
{
    DumpRenderTreeSupportQt::whiteListAccessFromOrigin(sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void LayoutTestController::removeOriginAccessWhitelistEntry(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains)
{
    DumpRenderTreeSupportQt::removeWhiteListAccessFromOrigin(sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void LayoutTestController::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    DumpRenderTreeSupportQt::setCustomPolicyDelegate(enabled, permissive);
}

void LayoutTestController::waitForPolicyDelegate()
{
    m_waitForPolicy = true;
    waitUntilDone();
}

void LayoutTestController::overridePreference(const QString& name, const QVariant& value)
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
    else
        printf("ERROR: LayoutTestController::overridePreference() does not support the '%s' preference\n",
            name.toLatin1().data());
}

void LayoutTestController::setUserStyleSheetLocation(const QString& url)
{
    m_userStyleSheetLocation = QUrl(url);

    if (m_userStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void LayoutTestController::setCaretBrowsingEnabled(bool value)
{
    DumpRenderTreeSupportQt::setCaretBrowsingEnabled(m_drt->webPage(), value);
}

void LayoutTestController::setUserStyleSheetEnabled(bool enabled)
{
    m_userStyleSheetEnabled = enabled;

    if (enabled)
        m_drt->webPage()->settings()->setUserStyleSheetUrl(m_userStyleSheetLocation);
    else
        m_drt->webPage()->settings()->setUserStyleSheetUrl(QUrl());
}

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme)
{
    DumpRenderTreeSupportQt::setDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

int LayoutTestController::workerThreadCount()
{
    return DumpRenderTreeSupportQt::workerThreadCount();
}

int LayoutTestController::pageNumberForElementById(const QString& id, float width, float height)
{
    // If no size specified, webpage viewport size is used
    if (!width && !height) {
        width = m_drt->webPage()->viewportSize().width();
        height = m_drt->webPage()->viewportSize().height();
    }

    return DumpRenderTreeSupportQt::pageNumberForElementById(m_drt->webPage()->mainFrame(), id, width, height);
}

int LayoutTestController::numberOfPages(float width, float height)
{
    return DumpRenderTreeSupportQt::numberOfPages(m_drt->webPage()->mainFrame(), width, height);
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    return DumpRenderTreeSupportQt::shouldClose(m_drt->webPage()->mainFrame());
}

void LayoutTestController::setScrollbarPolicy(const QString& orientation, const QString& policy)
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

void LayoutTestController::setSmartInsertDeleteEnabled(bool enable)
{
    DumpRenderTreeSupportQt::setSmartInsertDeleteEnabled(m_drt->webPage(), enable);
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool enable)
{
    DumpRenderTreeSupportQt::setSelectTrailingWhitespaceEnabled(m_drt->webPage(), enable);
}

void LayoutTestController::execCommand(const QString& name, const QString& value)
{
    DumpRenderTreeSupportQt::executeCoreCommandByName(m_drt->webPage(), name, value);
}

bool LayoutTestController::isCommandEnabled(const QString& name) const
{
    return DumpRenderTreeSupportQt::isCommandEnabled(m_drt->webPage(), name);
}

QString LayoutTestController::markerTextForListItem(const QWebElement& listItem)
{
    return DumpRenderTreeSupportQt::markerTextForListItem(listItem);
}

QVariantMap LayoutTestController::computedStyleIncludingVisitedInfo(const QWebElement& element) const
{
    return DumpRenderTreeSupportQt::computedStyleIncludingVisitedInfo(element);
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(const QString& elementId)
{
    return DumpRenderTreeSupportQt::elementDoesAutoCompleteForElementWithId(m_drt->webPage()->mainFrame(), elementId);
}

void LayoutTestController::authenticateSession(const QString&, const QString&, const QString&)
{
    // FIXME: If there is a concept per-session (per-process) credential storage, the credentials should be added to it for later use.
}

void LayoutTestController::setIconDatabaseEnabled(bool enable)
{
    if (enable && !m_drt->persistentStoragePath().isEmpty())
        QWebSettings::setIconDatabasePath(m_drt->persistentStoragePath());
    else
        QWebSettings::setIconDatabasePath(QString());
}

void LayoutTestController::setEditingBehavior(const QString& editingBehavior)
{
    DumpRenderTreeSupportQt::setEditingBehavior(m_drt->webPage(), editingBehavior);
}

void LayoutTestController::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    // FIXME: Implement for DeviceOrientation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=30335.
}

void LayoutTestController::setGeolocationPermission(bool allow)
{
    setGeolocationPermissionCommon(allow);
    emit geolocationPermissionSet();
}

void LayoutTestController::setGeolocationPermissionCommon(bool allow)
{
     m_isGeolocationPermissionSet = true;
     m_geolocationPermission = allow;
}

void LayoutTestController::setMockGeolocationError(int code, const QString& message)
{
    DumpRenderTreeSupportQt::setMockGeolocationError(code, message);
}

void LayoutTestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    DumpRenderTreeSupportQt::setMockGeolocationPosition(latitude, longitude, accuracy);
}

void LayoutTestController::setMockSpeechInputResult(const QString& result)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void LayoutTestController::evaluateScriptInIsolatedWorld(int worldID, const QString& script)
{
    DumpRenderTreeSupportQt::evaluateScriptInIsolatedWorld(m_drt->webPage()->mainFrame(), worldID, script);
}

bool LayoutTestController::isPageBoxVisible(int pageIndex)
{
    return DumpRenderTreeSupportQt::isPageBoxVisible(m_drt->webPage()->mainFrame(), pageIndex);
}

QString LayoutTestController::pageSizeAndMarginsInPixels(int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    return DumpRenderTreeSupportQt::pageSizeAndMarginsInPixels(m_drt->webPage()->mainFrame(), pageIndex,
                                                               width, height, marginTop, marginRight, marginBottom, marginLeft);
}

QString LayoutTestController::pageProperty(const QString& propertyName, int pageNumber)
{
    return DumpRenderTreeSupportQt::pageProperty(m_drt->webPage()->mainFrame(), propertyName, pageNumber);
}

void LayoutTestController::addUserStyleSheet(const QString& sourceCode)
{
    DumpRenderTreeSupportQt::addUserStyleSheet(m_drt->webPage(), sourceCode);
}

void LayoutTestController::removeAllVisitedLinks()
{
    QWebHistory* history = m_drt->webPage()->history();
    history->clear();
    DumpRenderTreeSupportQt::dumpVisitedLinksCallbacks(true);
}

const unsigned LayoutTestController::maxViewWidth = 800;
const unsigned LayoutTestController::maxViewHeight = 600;
