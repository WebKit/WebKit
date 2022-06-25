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

#include <wtf/Vector.h>

namespace WebCore {

class OrientationNotifier {
public:
    explicit OrientationNotifier(int orientation) { m_orientation = orientation; }
    ~OrientationNotifier();

    void orientationChanged(int orientation);

    class Observer {
    public:
        virtual ~Observer();
        virtual void orientationChanged(int orientation) = 0;
        void setNotifier(OrientationNotifier*);

    private:
        OrientationNotifier* m_notifier { nullptr };
    };

    void addObserver(Observer&);
    void removeObserver(Observer&);
    int orientation() const { return m_orientation; }

private:
    Vector<std::reference_wrapper<Observer>> m_observers;
    int m_orientation;
};

inline OrientationNotifier::~OrientationNotifier()
{
    for (Observer& observer : m_observers)
        observer.setNotifier(nullptr);
}

inline OrientationNotifier::Observer::~Observer()
{
    if (m_notifier)
        m_notifier->removeObserver(*this);
}

inline void OrientationNotifier::Observer::setNotifier(OrientationNotifier* notifier)
{
    if (m_notifier == notifier)
        return;

    if (m_notifier && notifier)
        m_notifier->removeObserver(*this);

    ASSERT(!m_notifier || !notifier);
    m_notifier = notifier;
}

inline void OrientationNotifier::orientationChanged(int orientation)
{
    m_orientation = orientation;
    for (Observer& observer : m_observers)
        observer.orientationChanged(orientation);
}

inline void OrientationNotifier::addObserver(Observer& observer)
{
    m_observers.append(observer);
    observer.setNotifier(this);
}

inline void OrientationNotifier::removeObserver(Observer& observer)
{
    m_observers.removeFirstMatching([&observer](auto item) {
        if (&observer != &item.get())
            return false;
        observer.setNotifier(nullptr);
        return true;
    });
}

}
