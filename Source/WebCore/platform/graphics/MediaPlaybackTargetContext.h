/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef MediaPlaybackTargetContext_h
#define MediaPlaybackTargetContext_h

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include <wtf/text/WTFString.h>

OBJC_CLASS AVOutputContext;

#if PLATFORM(COCOA)
OBJC_CLASS NSKeyedArchiver;
OBJC_CLASS NSKeyedUnarchiver;
#endif

namespace WebCore {

class MediaPlaybackTargetContext {
public:
    enum Type : int32_t {
        None,
        AVOutputContextType,
        MockType,
    };

    enum ContextState {
        Unknown = 0,
        OutputDeviceUnavailable = 1,
        OutputDeviceAvailable = 2,
    };
    typedef unsigned State;

    MediaPlaybackTargetContext()
        : m_type(None)
    {
    }

    MediaPlaybackTargetContext(AVOutputContext *outputContext)
        : m_type(AVOutputContextType)
        , m_outputContext(outputContext)
    {
    }

    MediaPlaybackTargetContext(const String& name, State state)
        : m_type(MockType)
        , m_name(name)
        , m_state(state)
    {
    }

    Type type() const { return m_type; }

    const String& mockDeviceName() const
    {
        ASSERT(m_type == MockType);
        return m_name;
    }

    State mockState() const
    {
        ASSERT(m_type == MockType);
        return m_state;
    }

    AVOutputContext *avOutputContext() const
    {
        ASSERT(m_type == AVOutputContextType);
        return m_outputContext;
    }

    bool encodingRequiresPlatformData() const { return m_type == AVOutputContextType; }
    
private:
    Type m_type { None };
    AVOutputContext *m_outputContext { nullptr };
    String m_name;
    State m_state { Unknown };
};

}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

#endif // MediaPlaybackTargetContext 
