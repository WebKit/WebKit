/*
 * Copyright (C) 2014 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ControlStates_h
#define ControlStates_h

#include <wtf/RetainPtr.h>

namespace WebCore {

#if PLATFORM(COCOA)
#ifndef __OBJC__
typedef struct objc_object *id;
#endif
typedef id PlatformControlInstance;
#endif

class ControlStates {
public:
    enum {
        HoverState = 1,
        PressedState = 1 << 1,
        FocusState = 1 << 2,
        EnabledState = 1 << 3,
        CheckedState = 1 << 4,
        ReadOnlyState = 1 << 5,
        DefaultState = 1 << 6,
        WindowInactiveState = 1 << 7,
        IndeterminateState = 1 << 8,
        SpinUpState = 1 << 9, // Sub-state for HoverState and PressedState.
        AllStates = 0xffffffff
    };

    typedef unsigned States;

    ControlStates(States states)
        : m_states(states)
        , m_initialized(false)
        , m_needsRepaint(false)
        , m_isDirty(false)
#if PLATFORM(COCOA)
        , m_controlInstance(nullptr)
#endif
    {
    }

    ControlStates()
        : ControlStates(0)
    {
    }

    ~ControlStates()
    {
    }

    States states() const { return m_states; }
    void setStates(States newStates)
    {
        if (newStates == m_states)
            return;
        m_states = newStates;
        m_isDirty = m_initialized;
        m_initialized = true;
    }

    bool needsRepaint() const { return m_needsRepaint; }
    void setNeedsRepaint(bool r) { m_needsRepaint = r; }

    bool isDirty() const { return m_isDirty; }
    void setDirty(bool d) { m_isDirty = d; }

#if PLATFORM(COCOA)
    PlatformControlInstance platformControl() const { return m_controlInstance.get(); }
    void setPlatformControl(PlatformControlInstance instance) { m_controlInstance = instance; }
#endif

private:
    States m_states;
    bool m_initialized;
    bool m_needsRepaint;
    bool m_isDirty;
#if PLATFORM(COCOA)
    RetainPtr<PlatformControlInstance> m_controlInstance;
#endif
};

}

#endif
