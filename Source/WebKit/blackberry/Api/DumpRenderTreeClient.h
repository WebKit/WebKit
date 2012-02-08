/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef DumpRenderTreeClient_h
#define DumpRenderTreeClient_h

#include "BlackBerryGlobal.h"

#include "PlatformString.h"
#include <JavaScriptCore/JSObjectRef.h>

namespace WebCore {
class Frame;
class DOMWrapperWorld;
class NavigationAction;
class Node;
class Range;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
}

namespace BlackBerry {
namespace WebKit {
class WebPage;

class BLACKBERRY_EXPORT DumpRenderTreeClient {
public:
    virtual void runTests() = 0;

    // FrameLoaderClient delegates
    virtual void didStartProvisionalLoadForFrame(WebCore::Frame*) = 0;
    virtual void didReceiveResponseForFrame(WebCore::Frame*, const WebCore::ResourceResponse&) = 0;
    virtual void didCommitLoadForFrame(WebCore::Frame*) = 0;
    virtual void didFailProvisionalLoadForFrame(WebCore::Frame*) = 0;
    virtual void didFailLoadForFrame(WebCore::Frame*) = 0;
    virtual void didFinishLoadForFrame(WebCore::Frame*) = 0;
    virtual void didFinishDocumentLoadForFrame(WebCore::Frame*) = 0;
    virtual void didClearWindowObjectInWorld(WebCore::DOMWrapperWorld*, JSGlobalContextRef, JSObjectRef windowObject) = 0;
    virtual void didReceiveTitleForFrame(const WTF::String& title, WebCore::Frame*) = 0;
    virtual void didDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&) = 0;
    virtual void didDispatchWillPerformClientRedirect() = 0;
    virtual void didHandleOnloadEventsForFrame(WebCore::Frame*) = 0;

    // ChromeClient delegates
    virtual void addMessageToConsole(const WTF::String& message, unsigned int lineNumber, const WTF::String& sourceID) = 0;
    virtual void runJavaScriptAlert(const WTF::String& message) = 0;
    virtual bool runJavaScriptConfirm(const WTF::String& message) = 0;
    virtual WTF::String runJavaScriptPrompt(const WTF::String& message, const WTF::String& defaultValue) = 0;
    virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message) = 0;
    virtual void setStatusText(const WTF::String&) = 0;
    virtual void exceededDatabaseQuota(WebCore::SecurityOrigin*, const WTF::String& name) = 0;
    virtual bool allowsOpeningWindow() = 0;
    virtual void windowCreated(WebPage*) = 0;

    // EditorClient delegates
    virtual void setAcceptsEditing(bool) = 0;
    virtual void didBeginEditing() = 0;
    virtual void didEndEditing() = 0;
    virtual void didChange() = 0;
    virtual void didChangeSelection() = 0;
    virtual bool shouldBeginEditingInDOMRange(WebCore::Range*) = 0;
    virtual bool shouldEndEditingInDOMRange(WebCore::Range*) = 0;
    virtual bool shouldDeleteDOMRange(WebCore::Range*) = 0;
    virtual bool shouldChangeSelectedDOMRangeToDOMRangeAffinityStillSelecting(WebCore::Range* fromRange, WebCore::Range* toRange, int affinity, bool stillSelecting) = 0;
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, int insertAction) = 0;
    virtual bool shouldInsertText(const WTF::String&, WebCore::Range*, int insertAction) = 0;
    virtual bool isSelectTrailingWhitespaceEnabled() const = 0;

};
}
}

#endif // DumpRenderTreeClient_h
