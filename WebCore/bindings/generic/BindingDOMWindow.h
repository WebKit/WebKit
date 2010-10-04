/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BindingDOMWindow_h
#define BindingDOMWindow_h

#include "DOMWindow.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GenericBinding.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "WindowFeatures.h"

namespace WebCore {

template <class Binding>
class BindingDOMWindow {
public:
    typedef typename Binding::Value BindingValue;

    static Frame* createWindow(State<Binding>*,
                               Frame* callingFrame,
                               Frame* enteredFrame,
                               Frame* openerFrame,
                               const String& url,
                               const String& frameName,
                               const WindowFeatures& windowFeatures,
                               BindingValue dialogArgs);

    static WebCore::DOMWindow* open(State<Binding>*,
                                    WebCore::DOMWindow* parent,
                                    const String& url,
                                    const String& frameName,
                                    const WindowFeatures& rawFeatures);

private:
    // Horizontal and vertical offset, from the parent content area,
    // around newly opened popups that don't specify a location.
    static const int popupTilePixels = 10;
};

// Implementations of templated methods must be in this file.

template <class Binding>
Frame* BindingDOMWindow<Binding>::createWindow(State<Binding>* state,
                                               Frame* callingFrame,
                                               Frame* enteredFrame,
                                               Frame* openerFrame,
                                               const String& url,
                                               const String& frameName,
                                               const WindowFeatures& windowFeatures,
                                               BindingValue dialogArgs)
{
    ASSERT(callingFrame);
    ASSERT(enteredFrame);

    ResourceRequest request;

    // For whatever reason, Firefox uses the entered frame to determine
    // the outgoingReferrer.  We replicate that behavior here.
    String referrer = enteredFrame->loader()->outgoingReferrer();
    request.setHTTPReferrer(referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, enteredFrame->loader()->outgoingOrigin());
    FrameLoadRequest frameRequest(request, frameName);

    // FIXME: It's much better for client API if a new window starts with a URL,
    // here where we know what URL we are going to open. Unfortunately, this
    // code passes the empty string for the URL, but there's a reason for that.
    // Before loading we have to set up the opener, openedByDOM,
    // and dialogArguments values. Also, to decide whether to use the URL
    // we currently do an allowsAccessFrom call using the window we create,
    // which can't be done before creating it. We'd have to resolve all those
    // issues to pass the URL instead of "".

    bool created;
    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    Frame* newFrame = WebCore::createWindow(callingFrame, openerFrame, frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->page()->setOpenedByDOM();

    Binding::DOMWindow::storeDialogArgs(state, newFrame, dialogArgs);

    if (!protocolIsJavaScript(url) || BindingSecurity<Binding>::canAccessFrame(state, newFrame, true)) {
        KURL completedUrl =
            url.isEmpty() ? KURL(ParsedURLString, "") : completeURL(state, url);
        bool userGesture = state->processingUserGesture();

        if (created)
            newFrame->loader()->changeLocation(completedUrl, referrer, false, false, userGesture);
        else if (!url.isEmpty())
            newFrame->navigationScheduler()->scheduleLocationChange(completedUrl.string(), referrer, false, false, userGesture);
    }

    return newFrame;
}

template<class Binding>
WebCore::DOMWindow* BindingDOMWindow<Binding>::open(State<Binding>* state,
                                                    WebCore::DOMWindow* parent,
                                                    const String& urlString,
                                                    const String& frameName,
                                                    const WindowFeatures& rawFeatures)
{
    Frame* frame = parent->frame();

    if (!BindingSecurity<Binding>::canAccessFrame(state, frame, true))
        return 0;

    Frame* firstFrame = state->getFirstFrame();
    if (!firstFrame)
        return 0;

    Frame* activeFrame = state->getActiveFrame();
    // We may not have a calling context if we are invoked by a plugin
    // via NPAPI.
    if (!activeFrame)
        activeFrame = firstFrame;

    Page* page = frame->page();
    if (!page)
        return 0;

    // Because FrameTree::find() returns true for empty strings, we must check
    // for empty framenames. Otherwise, illegitimate window.open() calls with
    // no name will pass right through the popup blocker.
    if (!BindingSecurity<Binding>::allowPopUp(state)
        && (frameName.isEmpty() || !frame->tree()->find(frameName))) {
        return 0;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we can schedule a location change right now and
    // return early.
    bool topOrParent = false;
    if (frameName == "_top") {
        frame = frame->tree()->top();
        topOrParent = true;
    } else if (frameName == "_parent") {
        if (Frame* parent = frame->tree()->parent())
            frame = parent;
        topOrParent = true;
    }
    if (topOrParent) {
        if (!BindingSecurity<Binding>::shouldAllowNavigation(state, frame))
            return 0;

        String completedUrl;
        if (!urlString.isEmpty())
            completedUrl = completeURL(state, urlString);

        if (!completedUrl.isEmpty()
            && (!protocolIsJavaScript(completedUrl)
                || BindingSecurity<Binding>::canAccessFrame(state, frame, true))) {
            bool userGesture = state->processingUserGesture();

            // For whatever reason, Firefox uses the first frame to determine
            // the outgoingReferrer.  We replicate that behavior here.
            String referrer = firstFrame->loader()->outgoingReferrer();

            frame->navigationScheduler()->scheduleLocationChange(completedUrl, referrer, false, false, userGesture);
        }
        return frame->domWindow();
    }

    // In the case of a named frame or a new window, we'll use the
    // createWindow() helper.

    // Work with a copy of the parsed values so we can restore the
    // values we may not want to overwrite after we do the multiple
    // monitor fixes.
    WindowFeatures windowFeatures(rawFeatures);
    FloatRect screenRect = screenAvailableRect(page->mainFrame()->view());

    // Set default size and location near parent window if none were specified.
    // These may be further modified by adjustWindowRect, below.
    if (!windowFeatures.xSet) {
        windowFeatures.x = parent->screenX() - screenRect.x() + popupTilePixels;
        windowFeatures.xSet = true;
    }
    if (!windowFeatures.ySet) {
        windowFeatures.y = parent->screenY() - screenRect.y() + popupTilePixels;
        windowFeatures.ySet = true;
    }
    if (!windowFeatures.widthSet) {
        windowFeatures.width = parent->innerWidth();
        windowFeatures.widthSet = true;
    }
    if (!windowFeatures.heightSet) {
        windowFeatures.height = parent->innerHeight();
        windowFeatures.heightSet = true;
    }

    FloatRect windowRect(windowFeatures.x, windowFeatures.y, windowFeatures.width, windowFeatures.height);

    // The new window's location is relative to its current screen, so shift
    // it in case it's on a secondary monitor. See http://b/viewIssue?id=967905.
    windowRect.move(screenRect.x(), screenRect.y());
    WebCore::DOMWindow::adjustWindowRect(screenRect, windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    // If either of the origin coordinates or dimensions weren't set
    // in the original string, make sure they aren't set now.
    if (!rawFeatures.xSet) {
        windowFeatures.x = 0;
        windowFeatures.xSet = false;
    }
    if (!rawFeatures.ySet) {
        windowFeatures.y = 0;
        windowFeatures.ySet = false;
    }
    if (!rawFeatures.widthSet) {
      windowFeatures.width = 0;
      windowFeatures.widthSet = false;
    }
    if (!rawFeatures.heightSet) {
      windowFeatures.height = 0;
      windowFeatures.heightSet = false;
    }

    frame = createWindow(state, activeFrame, firstFrame, frame, urlString, frameName, windowFeatures, Binding::emptyScriptValue());

    if (!frame)
        return 0;

    return frame->domWindow();
}

} // namespace WebCore

#endif // BindingDOMWindow_h
