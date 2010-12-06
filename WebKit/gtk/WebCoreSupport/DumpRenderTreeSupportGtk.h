/*
 *  Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef DumpRenderTreeSupportGtk_h
#define DumpRenderTreeSupportGtk_h


#include "JSStringRef.h"

#include <atk/atk.h>
#include <glib.h>
#include <webkit/webkitdefines.h>
#include <webkit/webkitwebframe.h>
#include <wtf/text/CString.h>

class DumpRenderTreeSupportGtk {

public:
    DumpRenderTreeSupportGtk();
    ~DumpRenderTreeSupportGtk();

    static void setDumpRenderTreeModeEnabled(bool);
    static bool dumpRenderTreeModeEnabled();

    static void setLinksIncludedInFocusChain(bool);
    static bool linksIncludedInFocusChain();
    static JSValueRef nodesFromRect(JSContextRef context, JSValueRef value, int x, int y, unsigned top, unsigned right, unsigned bottom, unsigned left, bool ignoreClipping);

    // FIXME: Move these to webkitwebframe.h once their API has been discussed.
    static GSList* getFrameChildren(WebKitWebFrame* frame);
    static WTF::CString getInnerText(WebKitWebFrame* frame);
    static WTF::CString dumpRenderTree(WebKitWebFrame* frame);
    static WTF::CString counterValueForElementById(WebKitWebFrame* frame, const char* id);
    static int pageNumberForElementById(WebKitWebFrame* frame, const char* id, float pageWidth, float pageHeight);
    static int numberOfPagesForFrame(WebKitWebFrame* frame, float pageWidth, float pageHeight);
    static guint getPendingUnloadEventCount(WebKitWebFrame* frame);
    static bool pauseAnimation(WebKitWebFrame* frame, const char* name, double time, const char* element);
    static bool pauseTransition(WebKitWebFrame* frame, const char* name, double time, const char* element);
    static bool pauseSVGAnimation(WebKitWebFrame* frame, const char* animationId, double time, const char* elementId);
    static WTF::CString markerTextForListItem(WebKitWebFrame* frame, JSContextRef context, JSValueRef nodeObject);
    static unsigned int numberOfActiveAnimations(WebKitWebFrame* frame);
    static void suspendAnimations(WebKitWebFrame* frame);
    static void resumeAnimations(WebKitWebFrame* frame);
    static void clearMainFrameName(WebKitWebFrame* frame);
    static AtkObject* getFocusedAccessibleElement(WebKitWebFrame* frame);

    // WebKitWebView
    static void executeCoreCommandByName(WebKitWebView* webView, const gchar* name, const gchar* value);
    static bool isCommandEnabled(WebKitWebView* webView, const gchar* name);

    static void whiteListAccessFromOrigin(const gchar* sourceOrigin, const gchar* destinationProtocol, const gchar* destinationHost, bool allowDestinationSubdomains);
    static void resetOriginAccessWhiteLists();

private:
    static bool s_drtRun;
    static bool s_linksIncludedInTabChain;
};

#endif
