/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebCompositorInputHandlerImpl_h
#define WebCompositorInputHandlerImpl_h

#include "CCGestureCurve.h"
#include "CCInputHandler.h"
#include "WebActiveWheelFlingParameters.h"
#include "WebCompositorInputHandler.h"
#include "WebInputEvent.h"
#include <public/WebCompositor.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WTF {
class Mutex;
}

namespace WebCore {
class IntPoint;
class CCGestureCurveTarget;
class CCInputHandlerClient;
class CCThread;
}

namespace WebKit {

class WebCompositorInputHandlerClient;

class WebCompositorInputHandlerImpl : public WebCompositorInputHandler, public WebCore::CCInputHandler, public WebCore::CCGestureCurveTarget {
    WTF_MAKE_NONCOPYABLE(WebCompositorInputHandlerImpl);
public:
    static PassOwnPtr<WebCompositorInputHandlerImpl> create(WebCore::CCInputHandlerClient*);
    static WebCompositorInputHandler* fromIdentifier(int identifier);

    virtual ~WebCompositorInputHandlerImpl();

    // WebCompositorInputHandler implementation.
    virtual void setClient(WebCompositorInputHandlerClient*);
    virtual void handleInputEvent(const WebInputEvent&);

    // WebCore::CCInputHandler implementation.
    virtual int identifier() const;
    virtual void animate(double monotonicTime);

    // WebCore::CCGestureCurveTarget implementation.
    virtual void scrollBy(const WebCore::IntPoint&);

private:
    explicit WebCompositorInputHandlerImpl(WebCore::CCInputHandlerClient*);

    enum EventDisposition { DidHandle, DidNotHandle, DropEvent };
    // This function processes the input event and determines the disposition, but does not make
    // any calls out to the WebCompositorInputHandlerClient. Some input types defer to helpers.
    EventDisposition handleInputEventInternal(const WebInputEvent&);

    EventDisposition handleGestureFling(const WebGestureEvent&);

    // Returns true if we actually had an active fling to cancel.
    bool cancelCurrentFling();

    OwnPtr<WebCore::CCActiveGestureAnimation> m_wheelFlingAnimation;
    // Parameters for the active fling animation, stored in case we need to transfer it out later.
    WebActiveWheelFlingParameters m_wheelFlingParameters;

    WebCompositorInputHandlerClient* m_client;
    int m_identifier;
    WebCore::CCInputHandlerClient* m_inputHandlerClient;

#ifndef NDEBUG
    bool m_expectScrollUpdateEnd;
    bool m_expectPinchUpdateEnd;
#endif
    bool m_gestureScrollStarted;

    static int s_nextAvailableIdentifier;
    static HashSet<WebCompositorInputHandlerImpl*>* s_compositors;
};

}

#endif // WebCompositorImpl_h
