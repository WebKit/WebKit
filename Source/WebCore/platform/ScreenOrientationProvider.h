/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ScreenOrientationType.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/RetainPtr.h>
#include <wtf/WeakObjCPtr.h>

OBJC_CLASS UIWindow;
OBJC_CLASS WebScreenOrientationObserver;
#endif

namespace WebCore {

class ScreenOrientationProvider : public RefCounted<ScreenOrientationProvider>, public CanMakeWeakPtr<ScreenOrientationProvider> {
public:
    WEBCORE_EXPORT static Ref<ScreenOrientationProvider> create();
    WEBCORE_EXPORT ~ScreenOrientationProvider();

    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() { }
        virtual void screenOrientationDidChange(ScreenOrientationType) = 0;
    };

    WEBCORE_EXPORT ScreenOrientationType currentOrientation();
    WEBCORE_EXPORT void addObserver(Observer&);
    WEBCORE_EXPORT void removeObserver(Observer&);

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT void setWindow(UIWindow *);
#endif

    void screenOrientationDidChange();

private:
    explicit ScreenOrientationProvider();

    std::optional<ScreenOrientationType> platformCurrentOrientation();
    void platformStartListeningForChanges();
    void platformStopListeningForChanges();

    WeakHashSet<Observer> m_observers;
    std::optional<ScreenOrientationType> m_currentOrientation;

#if PLATFORM(IOS_FAMILY)
    WeakObjCPtr<UIWindow> m_window;
    RetainPtr<WebScreenOrientationObserver> m_systemObserver;
#endif
};

} // namespace WebCore
