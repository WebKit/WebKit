/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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

#ifndef GestureRecognizerChromium_h
#define GestureRecognizerChromium_h

#include "PlatformGestureRecognizer.h"

#include "PlatformTouchEvent.h"
#include "TouchEvent.h"
#include <wtf/HashMap.h>

namespace WebCore {

class InnerGestureRecognizer {
public:
    enum State {
        NoGesture,
        PendingSyntheticClick,
        Scroll
    };

    typedef bool (*GestureTransitionFunction)(InnerGestureRecognizer*, const PlatformTouchPoint&);

    ~InnerGestureRecognizer();

    void dispatchSyntheticClick(const PlatformTouchPoint&);
    virtual void reset();
    bool isInClickTimeWindow();
    bool isInsideManhattanSquare(const PlatformTouchPoint&);
    virtual bool processTouchEventForGesture(const PlatformTouchEvent&, EventHandler*, bool handled);
    void scrollViaTouchMotion(const PlatformTouchPoint&);
    void setState(State value) { m_state = value; }
    State state() { return m_state; }
protected:
    InnerGestureRecognizer();
    void addEdgeFunction(State, unsigned finger, PlatformTouchPoint::State, bool touchHandledByJavaScript, GestureTransitionFunction);
    void updateValues(double touchTime, const PlatformTouchPoint&);
    static unsigned int signature(State, unsigned, PlatformTouchPoint::State, bool);

    friend class GestureRecognizerChromium;

    WTF::HashMap<int, GestureTransitionFunction> m_edgeFunctions;
    IntPoint m_firstTouchPosition;
    double m_firstTouchTime;
    State m_state;
    double m_lastTouchTime;
    EventHandler* m_eventHandler;

    bool m_ctrlKey;
    bool m_altKey;
    bool m_shiftKey;
    bool m_metaKey;
};

class GestureRecognizerChromium : public PlatformGestureRecognizer  {
public:

    GestureRecognizerChromium();
    virtual ~GestureRecognizerChromium();

    virtual void reset()
    {
        m_innerGestureRecognizer.reset();
    };
 
    virtual bool processTouchEventForGesture(const PlatformTouchEvent& touchEvent, EventHandler* eventHandler, bool handled)
    {
        return m_innerGestureRecognizer.processTouchEventForGesture(touchEvent, eventHandler, handled);
    }
private:
    InnerGestureRecognizer m_innerGestureRecognizer;
};

} // namespace WebCore

#endif // GestureRecognizerChromium_h
