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

#include "config.h"

#include "DumpRenderTreeClient.h"
#include "FrameLoaderClientHaiku.h"

#include "WebFramePrivate.h"
#include "WebPage.h"
#include "WebView.h"

#include <Bitmap.h>
#include "WebCore/DocumentLoader.h"
#include "WebCore/Document.h"
#include "WebCore/DOMWindow.h"
#include "WebCore/FocusController.h"
#include "WebCore/Frame.h"
#include "WebCore/FrameLoader.h"
#include "WebCore/FrameView.h"
#include "WebCore/NotImplemented.h"
#include "WebCore/Page.h"
#include "WebCore/PageGroup.h"
#include "WebCore/ScriptController.h"
#include "WebCore/Settings.h"
#include "WebCore/UserContentTypes.h"

#include "WebCore/Editor.h"

#include "WebFrame.h"

namespace WebCore {

bool DumpRenderTreeClient::s_drtRun = false;

void DumpRenderTreeClient::setDumpRenderTreeModeEnabled(bool enabled)
{
    s_drtRun = enabled;
}

bool DumpRenderTreeClient::dumpRenderTreeModeEnabled()
{
    return s_drtRun;
}

void DumpRenderTreeClient::Register(BWebPage* page)
{
    page->fDumpRenderTree = this;
}

void DumpRenderTreeClient::addUserScript(const BWebView* view,
    const String& sourceCode, bool runAtStart, bool allFrames)
{
    // FIXME this was moved to viewGroup and this method should be public
    // and part of the WebView API to match mac/ios/win.
#if 0
    view->WebPage()->page()->group().addUserScriptToWorld(
        WebCore::mainThreadNormalWorld(), sourceCode, WebCore::URL(),
        Vector<String>(), Vector<String>(),
        runAtStart ? WebCore::InjectAtDocumentStart : WebCore::InjectAtDocumentEnd,
        allFrames ? WebCore::InjectInAllFrames : WebCore::InjectInTopFrameOnly);
#endif
}

void DumpRenderTreeClient::clearUserScripts(const BWebView* view)
{
#if 0
    view->WebPage()->page()->group().removeUserScriptsFromWorld(
        WebCore::mainThreadNormalWorld());
#endif
}


void DumpRenderTreeClient::executeCoreCommandByName(const BWebView* view,
        const BString name, const BString value)
{
    view->WebPage()->page()->focusController().focusedOrMainFrame().editor().command(name).execute(value);
}


void
DumpRenderTreeClient::setShouldTrackVisitedLinks(bool)
{
    notImplemented();
}


unsigned DumpRenderTreeClient::pendingUnloadEventCount(const BWebFrame* frame)
{
    return frame->Frame()->document()->domWindow()->pendingUnloadEventListeners();
}


String DumpRenderTreeClient::responseMimeType(const BWebFrame* frame)
{
    WebCore::DocumentLoader *documentLoader = frame->Frame()->loader().documentLoader();

    if (!documentLoader)
        return String();

    return documentLoader->responseMIMEType();
}

// Compare with "WebKit/Tools/DumpRenderTree/mac/FrameLoadDelegate.mm
String DumpRenderTreeClient::suitableDRTFrameName(const BWebFrame* frame)
{
    const String frameName(frame->Frame()->tree().uniqueName());

    if (frame->Frame() == &frame->fData->page->mainFrame()) {
        if (!frameName.isEmpty())
            return String("main frame \"") + frameName + String("\"");

        return String("main frame");
    }

    if (!frameName.isEmpty())
        return String("frame \"") + frameName + String("\"");

    return String("frame (anonymous)");
}

BBitmap* DumpRenderTreeClient::getOffscreen(BWebView* view)
{
    view->OffscreenView()->LockLooper();
    view->OffscreenView()->Sync();
    view->OffscreenView()->UnlockLooper();
    return view->OffscreenBitmap();
}

BList DumpRenderTreeClient::frameChildren(BWebFrame* webFrame)
{
    WebCore::Frame* frame = webFrame->Frame();

    BList childFrames;

    for (unsigned index = 0; index < frame->tree().childCount(); index++) {
        WebCore::Frame *childFrame = frame->tree().child(index);
        WebCore::FrameLoaderClientHaiku& client = static_cast<WebCore::FrameLoaderClientHaiku&>(childFrame->loader().client());

        childFrames.AddItem(client.webFrame());
    }

    return childFrames;
}

void
DumpRenderTreeClient::setValueForUser(OpaqueJSContext const*, OpaqueJSValue const*, WTF::String const&)
{
    notImplemented();
}

void
DumpRenderTreeClient::setDomainRelaxationForbiddenForURLScheme(bool, WTF::String const&)
{
    notImplemented();
}

void
DumpRenderTreeClient::setSerializeHTTPLoads(bool)
{
    notImplemented();
}

void
DumpRenderTreeClient::setMockScrollbarsEnabled(bool enable)
{
    //WebCore::Settings::setMockScrollbarsEnabled(enable);
}

}
