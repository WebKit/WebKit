/*
    Copyright (C) 2010 Robert Hogan <robert@roberthogan.net>
    Copyright (C) 2008,2009,2010 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.
    Copyright (C) 2007 Apple Inc.

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
*/

#ifndef DumpRenderTreeSupportQt_h
#define DumpRenderTreeSupportQt_h

#include "qwebkitglobal.h"
#include <QNetworkCookieJar>
#include <QVariant>

typedef const struct OpaqueJSContext* JSContextRef;

namespace WebCore {
class Text;
class Node;
}

namespace JSC {
namespace Bindings {
class QtDRTNodeRuntime;
}
}

class QWebElement;
class QWebFrame;
class QWebPage;
class QWebHistoryItem;
class QWebScriptWorld;

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

extern QMap<int, QWebScriptWorld*> m_worldMap;

// Used to pass WebCore::Node's to layout tests using TestRunner
class QWEBKIT_EXPORT QDRTNode {
public:
    QDRTNode();
    QDRTNode(const QDRTNode&);
    QDRTNode &operator=(const QDRTNode&);
    ~QDRTNode();

private:
    explicit QDRTNode(WebCore::Node*);

    friend class DumpRenderTreeSupportQt;

    friend class QtDRTNodeRuntime;

    WebCore::Node* m_node;
};

Q_DECLARE_METATYPE(QDRTNode)

class QtDRTNodeRuntime {
public:
    static QDRTNode create(WebCore::Node*);
    static WebCore::Node* get(const QDRTNode&);
    static void initialize();
};

class QWEBKIT_EXPORT DumpRenderTreeSupportQt {

public:

    DumpRenderTreeSupportQt();
    ~DumpRenderTreeSupportQt();

    static void initialize();

    static void executeCoreCommandByName(QWebPage* page, const QString& name, const QString& value);
    static bool isCommandEnabled(QWebPage* page, const QString& name);
    static bool findString(QWebPage* page, const QString& string, const QStringList& optionArray);
    static void setSmartInsertDeleteEnabled(QWebPage* page, bool enabled);
    static void setSelectTrailingWhitespaceEnabled(QWebPage* page, bool enabled);
    static QVariantList selectedRange(QWebPage* page);
    static QVariantList firstRectForCharacterRange(QWebPage* page, int location, int length);
    static void confirmComposition(QWebPage*, const char* text);

    static bool pauseAnimation(QWebFrame*, const QString& name, double time, const QString& elementId);
    static bool pauseTransitionOfProperty(QWebFrame*, const QString& name, double time, const QString& elementId);
    static void suspendActiveDOMObjects(QWebFrame* frame);
    static void resumeActiveDOMObjects(QWebFrame* frame);

    static void setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme);
    static void setFrameFlatteningEnabled(QWebPage*, bool);
    static void setMockScrollbarsEnabled(QWebPage*, bool);
    static void setCaretBrowsingEnabled(QWebPage* page, bool value);
    static void setAuthorAndUserStylesEnabled(QWebPage*, bool);
    static void setMediaType(QWebFrame* qframe, const QString& type);
    static void setDumpRenderTreeModeEnabled(bool b);

    static void garbageCollectorCollect();
    static void garbageCollectorCollectOnAlternateThread(bool waitUntilDone);
    static void setAutofilled(const QWebElement&, bool enabled);
    static void setValueForUser(const QWebElement&, const QString& value);
    static int javaScriptObjectsCount();
    static void clearScriptWorlds();
    static void evaluateScriptInIsolatedWorld(QWebFrame* frame, int worldID, const QString& script);

    static void webInspectorExecuteScript(QWebPage* page, long callId, const QString& script);
    static void webInspectorShow(QWebPage* page);
    static void webInspectorClose(QWebPage* page);

    static QString webPageGroupName(QWebPage *page);
    static void webPageSetGroupName(QWebPage* page, const QString& groupName);
    static void clearFrameName(QWebFrame* frame);
    static void overwritePluginDirectories();
    static int numberOfActiveAnimations(QWebFrame*);
    static int numberOfPages(QWebFrame* frame, float width, float height);
    static bool hasDocumentElement(QWebFrame* frame);
    static bool elementDoesAutoCompleteForElementWithId(QWebFrame* frame, const QString& elementId);
    static void setWindowsBehaviorAsEditingBehavior(QWebPage*);

    static void clearAllApplicationCaches();

    static void whiteListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);
    static void removeWhiteListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);
    static void resetOriginAccessWhiteLists();

    static void setMockDeviceOrientation(QWebPage*, bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma);

    static void resetGeolocationMock(QWebPage*);
    static void setMockGeolocationPermission(QWebPage*, bool allowed);
    static void setMockGeolocationPosition(QWebPage*, double latitude, double longitude, double accuracy);
    static void setMockGeolocationError(QWebPage*, int errorCode, const QString& message);
    static int numberOfPendingGeolocationPermissionRequests(QWebPage*);

    static int workerThreadCount();

    static QString markerTextForListItem(const QWebElement& listItem);
    static QVariantMap computedStyleIncludingVisitedInfo(const QWebElement& element);

    static void dumpFrameLoader(bool b);
    static void dumpProgressFinishedCallback(bool);
    static void dumpUserGestureInFrameLoader(bool b);
    static void dumpResourceLoadCallbacks(bool b);
    static void dumpResourceResponseMIMETypes(bool b);
    static void dumpResourceLoadCallbacksPath(const QString& path);
    static void dumpWillCacheResponseCallbacks(bool);
    static void setWillSendRequestReturnsNullOnRedirect(bool b);
    static void setWillSendRequestReturnsNull(bool b);
    static void setWillSendRequestClearHeaders(const QStringList& headers);
    static void dumpHistoryCallbacks(bool b);
    static void dumpVisitedLinksCallbacks(bool b);

    static void setDeferMainResourceDataLoad(bool b);

    static void dumpEditingCallbacks(bool b);
    static void dumpSetAcceptsEditing(bool b);

    static void dumpNotification(bool b);

    static QMap<QString, QWebHistoryItem> getChildHistoryItems(const QWebHistoryItem& historyItem);
    static bool isTargetItem(const QWebHistoryItem& historyItem);
    static QString historyItemTarget(const QWebHistoryItem& historyItem);

    static bool shouldClose(QWebFrame* frame);

    static void setCustomPolicyDelegate(bool enabled, bool permissive);

    static QString pageSizeAndMarginsInPixels(QWebFrame* frame, int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft);
    static QString pageProperty(QWebFrame* frame, const QString& propertyName, int pageNumber);
    static void addUserStyleSheet(QWebPage* page, const QString& sourceCode);
    static void removeUserStyleSheets(QWebPage*);
    static void simulateDesktopNotificationClick(const QString& title);
    static QString viewportAsText(QWebPage*, int deviceDPI, const QSize& deviceSize, const QSize& availableSize);

    static void scalePageBy(QWebFrame*, float scale, const QPoint& origin);

    static QString responseMimeType(QWebFrame*);
    static void clearOpener(QWebFrame*);
    static void addURLToRedirect(const QString& origin, const QString& destination);
    static QStringList contextMenu(QWebPage*);

    static double defaultMinimumTimerInterval(); // Not really tied to WebView
    static void setMinimumTimerInterval(QWebPage*, double);

    static QUrl mediaContentUrlByElementId(QWebFrame*, const QString& elementId);
    static void setAlternateHtml(QWebFrame*, const QString& html, const QUrl& baseUrl, const QUrl& failingUrl);

    static QString layerTreeAsText(QWebFrame*);

    static void injectInternalsObject(QWebFrame*);
    static void injectInternalsObject(JSContextRef);
    static void resetInternalsObject(QWebFrame*);
    static void resetInternalsObject(JSContextRef);

    static void setInteractiveFormValidationEnabled(QWebPage*, bool);

    static void setDefersLoading(QWebPage*, bool flag);
    static void goBack(QWebPage*);

    static bool thirdPartyCookiePolicyAllows(QWebPage*, const QUrl&, const QUrl& firstPartyUrl);

    static QImage paintPagesWithBoundaries(QWebFrame*);
};

#endif
