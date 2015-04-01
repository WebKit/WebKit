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

#ifndef MediaPlaybackTarget_h
#define MediaPlaybackTarget_h

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include <wtf/RetainPtr.h>

OBJC_CLASS NSKeyedArchiver;
OBJC_CLASS NSKeyedUnarchiver;
OBJC_CLASS AVOutputContext;

namespace WebCore {

class MediaPlaybackTarget {
public:
    virtual ~MediaPlaybackTarget() { }

#if PLATFORM(MAC)
    WEBCORE_EXPORT MediaPlaybackTarget(AVOutputContext *context = nil) { m_devicePickerContext = context; }

    WEBCORE_EXPORT void encode(NSKeyedArchiver *) const;
    WEBCORE_EXPORT static bool decode(NSKeyedUnarchiver *, MediaPlaybackTarget&);

    void setDevicePickerContext(AVOutputContext *context) { m_devicePickerContext = context; }
    AVOutputContext *devicePickerContext() const { return m_devicePickerContext.get(); }
    bool hasActiveRoute() const;
#else
    void setDevicePickerContext(AVOutputContext *) { }
    AVOutputContext *devicePickerContext() const { return nullptr; }
    bool hasActiveRoute() const { return false; }
#endif

protected:
#if PLATFORM(MAC)
    RetainPtr<AVOutputContext> m_devicePickerContext;
#endif
};

}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

#endif
