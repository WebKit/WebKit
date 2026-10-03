/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#ifdef __cplusplus

#if PLATFORM(MAC)

#import "ClassMethodSwizzler.h"

OBJC_CLASS NSEvent;

namespace TestWebKitAPI {

// While alive, +[NSEvent addLocalMonitorForEventsMatchingMask:handler:] doesn't install a real monitor. It keeps the
// handler of the most recently added monitor instead, so a test can send events straight to it (for example, to
// WKWebView's flags-changed monitor).
class LocalEventMonitorSwizzler {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LocalEventMonitorSwizzler);
    WTF_MAKE_NONCOPYABLE(LocalEventMonitorSwizzler);
public:
    LocalEventMonitorSwizzler();

    NSEvent *sendEventToMonitor(NSEvent *);

private:
    ClassMethodSwizzler m_swizzler;
};

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)

#endif // __cplusplus
