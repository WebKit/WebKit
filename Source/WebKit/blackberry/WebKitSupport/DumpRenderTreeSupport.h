/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#ifndef DumpRenderTreeSupport_h
#define DumpRenderTreeSupport_h

#include <JavaScriptCore/JSObjectRef.h>

namespace BlackBerry {
namespace WebKit {
class WebPage;
}
}

namespace WebCore {
class Frame;
}

namespace WTF {
class String;
}

class DumpRenderTreeSupport {
public:
    DumpRenderTreeSupport();
    ~DumpRenderTreeSupport();

    static void setLinksIncludedInFocusChain(bool);
    static bool linksIncludedInFocusChain();

    static int javaScriptObjectsCount();
    static void garbageCollectorCollect();
    static void garbageCollectorCollectOnAlternateThread(bool waitUntilDone);

    static void dumpConfigurationForViewport(WebCore::Frame* mainFrame, int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight);

    static int numberOfPendingGeolocationPermissionRequests(BlackBerry::WebKit::WebPage*);
    static void resetGeolocationMock(BlackBerry::WebKit::WebPage*);
    static void setMockGeolocationError(BlackBerry::WebKit::WebPage*, int errorCode, const WTF::String message);
    static void setMockGeolocationPermission(BlackBerry::WebKit::WebPage*, bool allowed);
    static void setMockGeolocationPosition(BlackBerry::WebKit::WebPage*, double latitude, double longitude, double accuracy);
    static void scalePageBy(BlackBerry::WebKit::WebPage*, float, float, float);
    static JSValueRef computedStyleIncludingVisitedInfo(JSContextRef, JSValueRef);

private:
    static bool s_linksIncludedInTabChain;
};

#endif
