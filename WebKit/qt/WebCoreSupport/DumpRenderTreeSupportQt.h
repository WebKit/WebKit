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
#include <QVariant>

class QWebElement;
class QWebFrame;
class QWebPage;
class QWebHistoryItem;
class QWebScriptWorld;

extern QMap<int, QWebScriptWorld*> m_worldMap;

class QWEBKIT_EXPORT DumpRenderTreeSupportQt {

public:

    DumpRenderTreeSupportQt();
    ~DumpRenderTreeSupportQt();


    static void executeCoreCommandByName(QWebPage* page, const QString& name, const QString& value);
    static bool isCommandEnabled(QWebPage* page, const QString& name);
    static void setSmartInsertDeleteEnabled(QWebPage* page, bool enabled);
    static void setSelectTrailingWhitespaceEnabled(QWebPage* page, bool enabled);
    static QVariantList selectedRange(QWebPage* page);
    static QVariantList firstRectForCharacterRange(QWebPage* page, int location, int length);

    static bool pauseAnimation(QWebFrame*, const QString& name, double time, const QString& elementId);
    static bool pauseTransitionOfProperty(QWebFrame*, const QString& name, double time, const QString& elementId);
    static bool pauseSVGAnimation(QWebFrame*, const QString& animationId, double time, const QString& elementId);
    static void suspendActiveDOMObjects(QWebFrame* frame);
    static void resumeActiveDOMObjects(QWebFrame* frame);

    static void setDomainRelaxationForbiddenForURLScheme(bool forbidden, const QString& scheme);
    static void setFrameFlatteningEnabled(QWebPage*, bool);
    static void setCaretBrowsingEnabled(QWebPage* page, bool value);
    static void setMediaType(QWebFrame* qframe, const QString& type);
    static void setDumpRenderTreeModeEnabled(bool b);

    static void garbageCollectorCollect();
    static void garbageCollectorCollectOnAlternateThread(bool waitUntilDone);
    static void setJavaScriptProfilingEnabled(QWebFrame*, bool enabled);
    static int javaScriptObjectsCount();
    static void clearScriptWorlds();
    static void evaluateScriptInIsolatedWorld(QWebFrame* frame, int worldID, const QString& script);

    static void setTimelineProfilingEnabled(QWebPage*, bool enabled);
    static void webInspectorExecuteScript(QWebPage* page, long callId, const QString& script);
    static void webInspectorShow(QWebPage* page);
    static void webInspectorClose(QWebPage* page);

    static QString webPageGroupName(QWebPage *page);
    static QString counterValueForElementById(QWebFrame* frame, const QString& id);
    static void webPageSetGroupName(QWebPage* page, const QString& groupName);
    static void clearFrameName(QWebFrame* frame);
    static void overwritePluginDirectories();
    static int numberOfActiveAnimations(QWebFrame*);
    static void suspendAnimations(QWebFrame*);
    static void resumeAnimations(QWebFrame*);
    static int numberOfPages(QWebFrame* frame, float width, float height);
    static int pageNumberForElementById(QWebFrame* frame, const QString& id, float width, float height);
    static bool hasDocumentElement(QWebFrame* frame);
    static bool elementDoesAutoCompleteForElementWithId(QWebFrame* frame, const QString& elementId);
    static void setEditingBehavior(QWebPage* page, const QString& editingBehavior);

    static void whiteListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);
    static void removeWhiteListAccessFromOrigin(const QString& sourceOrigin, const QString& destinationProtocol, const QString& destinationHost, bool allowDestinationSubdomains);
    static void resetOriginAccessWhiteLists();

    static void setMockGeolocationPosition(double latitude, double longitude, double accuracy);
    static void setMockGeolocationError(int errorCode, const QString& message);

    static int workerThreadCount();

    static QString markerTextForListItem(const QWebElement& listItem);
    static QVariantMap computedStyleIncludingVisitedInfo(const QWebElement& element);

    static void dumpFrameLoader(bool b);
    static void dumpResourceLoadCallbacks(bool b);
    static void dumpResourceResponseMIMETypes(bool b);
    static void dumpResourceLoadCallbacksPath(const QString& path);
    static void setWillSendRequestReturnsNullOnRedirect(bool b);
    static void setWillSendRequestReturnsNull(bool b);
    static void setWillSendRequestClearHeaders(const QStringList& headers);

    static void setDeferMainResourceDataLoad(bool b);

    static void dumpEditingCallbacks(bool b);
    static void dumpSetAcceptsEditing(bool b);

    static void dumpNotification(bool b);

    static QMap<QString, QWebHistoryItem> getChildHistoryItems(const QWebHistoryItem& historyItem);
    static bool isTargetItem(const QWebHistoryItem& historyItem);
    static QString historyItemTarget(const QWebHistoryItem& historyItem);

    static bool shouldClose(QWebFrame* frame);

    static void setCustomPolicyDelegate(bool enabled, bool permissive);

    static bool isPageBoxVisible(QWebFrame* frame, int pageIndex);

    static QString pageSizeAndMarginsInPixels(QWebFrame* frame, int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft);
    static QString pageProperty(QWebFrame* frame, const QString& propertyName, int pageNumber);
    static void addUserStyleSheet(QWebPage* page, const QString& sourceCode);
    static void simulateDesktopNotificationClick(const QString& title);
    static QString viewportAsText(QWebPage*, const QSize&);
};

#endif
