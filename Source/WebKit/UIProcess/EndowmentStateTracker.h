/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import <wtf/RetainPtr.h>
#import <wtf/WeakHashSet.h>

OBJC_CLASS NSSet;
OBJC_CLASS RBSProcessMonitor;

namespace WebKit {

class WebPageProxy;

class EndowmentStateTracker {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static EndowmentStateTracker& singleton();

    class Client : public CanMakeWeakPtr<Client> {
    public:
        virtual ~Client() = default;
        virtual void isUserFacingChanged(bool) { }
        virtual void isVisibleChanged(bool) { }
    };

    bool isVisible() const { return ensureState().isVisible; }
    bool isUserFacing() const { return ensureState().isUserFacing; }

    void addClient(Client&);
    void removeClient(Client&);

    static bool isApplicationForeground(pid_t);

private:
    friend class NeverDestroyed<EndowmentStateTracker>;
    EndowmentStateTracker() = default;

    void registerMonitorIfNecessary();

    struct State {
        bool isUserFacing;
        bool isVisible;
    };
    static State stateFromEndowments(NSSet *endowments);
    const State& ensureState() const;
    void setState(State&&);

    WeakHashSet<Client> m_clients;
    RetainPtr<RBSProcessMonitor> m_processMonitor;
    mutable std::optional<State> m_state;
};

}

#endif
