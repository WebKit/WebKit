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

#include "Frame.h"
#include "FrameLoadRequest.h"
#include "GenericBinding.h"
#include "Page.h"
#include "SecurityOrigin.h"

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
    // We pass in the opener frame here so it can be used for looking up the
    // frame name, in case the active frame is different from the opener frame,
    // and the name references a frame relative to the opener frame, for example
    // "_self" or "_parent".
    Frame* newFrame = callingFrame->loader()->createWindow(openerFrame->loader(), frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->page()->setOpenedByDOM();

    Binding::DOMWindow::storeDialogArgs(state, newFrame, dialogArgs);

    if (!protocolIsJavaScript(url) || BindingSecurity<Binding>::canAccessFrame(state, newFrame, true)) {
        KURL completedUrl =
            url.isEmpty() ? KURL(ParsedURLString, "") : completeURL(url);
        bool userGesture = processingUserGesture();

        if (created)
            newFrame->loader()->changeLocation(completedUrl, referrer, false, false, userGesture);
        else if (!url.isEmpty())
            newFrame->redirectScheduler()->scheduleLocationChange(completedUrl.string(), referrer, false, userGesture);
    }

    return newFrame;
}

} // namespace WebCore

#endif // BindingDOMWindow_h
