/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <WebCore/EventListener.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// https://dom.spec.whatwg.org/#concept-event-listener
class RegisteredEventListener : public RefCounted<RegisteredEventListener> {
public:
    struct Options {
        Options(bool capture = false, bool passive = false, bool once = false, bool trustedOnly = false)
            : capture(capture)
            , passive(passive)
            , once(once)
            , trustedOnly(trustedOnly)
        { }

        bool capture;
        bool passive;
        bool once;
        bool trustedOnly;
    };

    static Ref<RegisteredEventListener> create(Ref<EventListener>&& listener, const Options& options)
    {
        return adoptRef(*new RegisteredEventListener(WTFMove(listener), options));
    }

    EventListener& callback() const { return m_callback; }
    bool useCapture() const { return m_useCapture; }
    bool isPassive() const { return m_isPassive; }
    bool isOnce() const { return m_isOnce; }
    bool wasRemoved() const { return m_wasRemoved; }
    bool trustedOnly() const { return m_trustedOnly; }

    void markAsRemoved() { m_wasRemoved = true; }

private:
    RegisteredEventListener(Ref<EventListener>&& listener, const Options& options)
        : m_useCapture(options.capture)
        , m_isPassive(options.passive)
        , m_isOnce(options.once)
        , m_wasRemoved(false)
        , m_trustedOnly(options.trustedOnly)
        , m_callback(WTFMove(listener))
    {
    }

    bool m_useCapture : 1;
    bool m_isPassive : 1;
    bool m_isOnce : 1;
    bool m_wasRemoved : 1;
    bool m_trustedOnly : 1;
    const Ref<EventListener> m_callback;
};

} // namespace WebCore
