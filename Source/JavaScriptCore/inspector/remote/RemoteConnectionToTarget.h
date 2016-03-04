/*
 * Copyright (C) 2013, 2015 Apple Inc. All Rights Reserved.
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

#if ENABLE(REMOTE_INSPECTOR)

#ifndef RemoteConnectionToTarget_h
#define RemoteConnectionToTarget_h

#import "InspectorFrontendChannel.h"
#import "RemoteConnectionToTarget.h"
#import "RemoteInspector.h"
#import <mutex>
#import <wtf/BlockPtr.h>
#import <wtf/Lock.h>
#import <wtf/RetainPtr.h>
#import <wtf/ThreadSafeRefCounted.h>

OBJC_CLASS NSString;

namespace Inspector {

typedef Vector<BlockPtr<void ()>> RemoteTargetQueue;

class RemoteConnectionToTarget final : public ThreadSafeRefCounted<RemoteConnectionToTarget>, public FrontendChannel {
public:
    RemoteConnectionToTarget(RemoteControllableTarget*, NSString *connectionIdentifier, NSString *destination);
    virtual ~RemoteConnectionToTarget();

    // Main API.
    bool setup(bool isAutomaticInspection = false, bool automaticallyPause = false);
    virtual void sendMessageToTarget(NSString *);
    virtual void close();
    virtual void targetClosed();

    Optional<unsigned> targetIdentifier() const;
    NSString *connectionIdentifier() const;
    NSString *destination() const;

    Lock& queueMutex() { return m_queueMutex; }
    const RemoteTargetQueue& queue() const { return m_queue; }
    void clearQueue() { m_queue.clear(); }

    // FrontendChannel overrides.
    ConnectionType connectionType() const override { return ConnectionType::Remote; }
    bool sendMessageToFrontend(const String&) override;

private:
    void dispatchAsyncOnTarget(void (^block)());

    void setupRunLoop();
    void teardownRunLoop();
    void queueTaskOnPrivateRunLoop(void (^block)());

    // This connection from the RemoteInspector singleton to the InspectionTarget
    // can be used on multiple threads. So any access to the target
    // itself must take this mutex to ensure m_target is valid.
    Lock m_targetMutex;

    // If a target has a specific run loop it wants to evaluate on
    // we setup our run loop sources on that specific run loop.
    RetainPtr<CFRunLoopRef> m_runLoop;
    RetainPtr<CFRunLoopSourceRef> m_runLoopSource;
    RemoteTargetQueue m_queue;
    Lock m_queueMutex;

    RemoteControllableTarget* m_target { nullptr };
    RetainPtr<NSString> m_connectionIdentifier;
    RetainPtr<NSString> m_destination;
    bool m_connected { false };
};

} // namespace Inspector

#endif // RemoteConnectionToTarget_h

#endif // ENABLE(REMOTE_INSPECTOR)
