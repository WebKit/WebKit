/*
    Copyright (C) 2013 Haiku, Inc.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1.  Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
    2.  Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DumpRenderTreeClient_h
#define DumpRenderTreeClient_h

#include <List.h>
#include <Size.h>

#include "wtf/URL.h"
#include <JavaScriptCore/JSStringRef.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class DOMWrapperWorld;
class Frame;
class MessagePortChannel;
}

class BBitmap;
class BMessage;
class BWebFrame;
class BWebPage;
class BWebView;
class DumpRenderTreeApp;


namespace WebCore {

class __attribute__((visibility ("default"))) DumpRenderTreeClient {
public:
    virtual ~DumpRenderTreeClient() { }
    virtual void didClearWindowObjectInWorld(WebCore::DOMWrapperWorld&, JSGlobalContextRef, JSObjectRef windowObject) = 0;

    void Register(BWebPage* page);

    static void setDumpRenderTreeModeEnabled(bool);
    static bool dumpRenderTreeModeEnabled();

    static unsigned pendingUnloadEventCount(const BWebFrame* frame);
    static String responseMimeType(const BWebFrame* frame);
    static String suitableDRTFrameName(const BWebFrame* frame);
    static void setValueForUser(JSContextRef, JSValueRef nodeObject, const String& value);
    static BBitmap* getOffscreen(BWebView* view);
    static BList    frameChildren(BWebFrame* frame);

    static void setSeamlessIFramesEnabled(bool);


    static void garbageCollectorCollect();
    static void garbageCollectorCollectOnAlternateThread(bool waitUntilDone);
    static size_t javaScriptObjectsCount();

    static void setDeadDecodedDataDeletionInterval(double);

    static void setMockScrollbarsEnabled(bool);

    static void deliverAllMutationsIfNecessary();
    static void setDomainRelaxationForbiddenForURLScheme(bool forbidden,
        const String& scheme);
    static void setSerializeHTTPLoads(bool);
    static void setShouldTrackVisitedLinks(bool);
    
    static void addUserScript(const BWebView* view, const String& sourceCode,
        bool runAtStart, bool allFrames);
    static void clearUserScripts(const BWebView* view);
    static void executeCoreCommandByName(const BWebView* view,
        const BString name, const BString value);

    static void injectMouseEvent(BWebPage* target, BMessage* event);
    static void injectKeyEvent(BWebPage* target, BMessage* event);
private:
    static bool s_drtRun;
};

}

#endif
