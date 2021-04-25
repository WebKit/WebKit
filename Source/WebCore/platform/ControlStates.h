/*
 * Copyright (C) 2014-2018 Apple Inc. All Rights Reserved.
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

#include <wtf/OptionSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/Seconds.h>

#if PLATFORM(COCOA)
#ifndef __OBJC__
typedef struct objc_object *id;
#endif
#endif

namespace WebCore {

#if PLATFORM(COCOA)
typedef id PlatformControlInstance;
#endif

class ControlStates {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class States : uint16_t {
        Hovered = 1 << 0,
        Pressed = 1 << 1,
        Focused = 1 << 2,
        Enabled = 1 << 3,
        Checked = 1 << 4,
        Default = 1 << 5,
        WindowInactive = 1 << 6,
        Indeterminate = 1 << 7,
        SpinUp = 1 << 8, // Sub-state for HoverState and PressedState.
        Presenting = 1 << 9,
    };

    ControlStates(OptionSet<States> states)
        : m_states(states)
    {
    }

    ControlStates() = default;

    OptionSet<States> states() const { return m_states; }
    void setStates(OptionSet<States> newStates)
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

    Seconds timeSinceControlWasFocused() const { return m_timeSinceControlWasFocused; }
    void setTimeSinceControlWasFocused(Seconds time) { m_timeSinceControlWasFocused = time; }

#if PLATFORM(COCOA)
    PlatformControlInstance platformControl() const { return m_controlInstance.get(); }
    void setPlatformControl(PlatformControlInstance instance) { m_controlInstance = instance; }
#endif

private:
    OptionSet<States> m_states;
    bool m_initialized { false };
    bool m_needsRepaint { false };
    bool m_isDirty { false };
    Seconds m_timeSinceControlWasFocused { 0_s };
#if PLATFORM(COCOA)
    RetainPtr<PlatformControlInstance> m_controlInstance;
#endif
};

}

#endif
