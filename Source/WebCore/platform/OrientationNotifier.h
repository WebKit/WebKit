/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <WebCore/IntDegrees.h>
#include <wtf/AbstractThreadSafeRefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

enum class VideoFrameRotation : uint16_t;

class OrientationNotifier final {
    WTF_MAKE_TZONE_ALLOCATED(OrientationNotifier);
public:
    explicit OrientationNotifier(IntDegrees orientation)
        : m_orientation(orientation)
    {
    }
    ~OrientationNotifier() = default;

    void orientationChanged(IntDegrees orientation);
    void rotationAngleForCaptureDeviceChanged(const String&, VideoFrameRotation);

    class Observer : public AbstractThreadSafeRefCountedAndCanMakeWeakPtr {
    public:
        virtual ~Observer() = default;
        virtual void orientationChanged(IntDegrees orientation) = 0;
        virtual void rotationAngleForHorizonLevelDisplayChanged(const String&, VideoFrameRotation) { }
    };

    void addObserver(Observer&);
    void removeObserver(Observer&);
    IntDegrees orientation() const { return m_orientation; }

private:
    ThreadSafeWeakHashSet<Observer> m_observers;
    IntDegrees m_orientation;
};

inline void OrientationNotifier::orientationChanged(IntDegrees orientation)
{
    m_orientation = orientation;
    m_observers.forEach([orientation](auto& observer) {
        observer.orientationChanged(orientation);
    });
}

inline void OrientationNotifier::rotationAngleForCaptureDeviceChanged(const String& devicePersistentId, VideoFrameRotation orientation)
{
    m_observers.forEach([&](auto& observer) {
        observer.rotationAngleForHorizonLevelDisplayChanged(devicePersistentId, orientation);
    });
}

inline void OrientationNotifier::addObserver(Observer& observer)
{
    m_observers.add(observer);
}

inline void OrientationNotifier::removeObserver(Observer& observer)
{
    m_observers.remove(observer);
}

} // namespace WebCore
