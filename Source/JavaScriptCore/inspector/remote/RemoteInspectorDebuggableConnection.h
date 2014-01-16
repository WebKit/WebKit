/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#ifndef RemoteInspectorDebuggableConnection_h
#define RemoteInspectorDebuggableConnection_h

#import "InspectorFrontendChannel.h"
#import "RemoteInspectorDebuggable.h"
#import <dispatch/dispatch.h>
#import <wtf/RetainPtr.h>
#import <wtf/Threading.h>
#import <wtf/ThreadSafeRefCounted.h>

OBJC_CLASS NSString;

namespace Inspector {

class RemoteInspectorDebuggableConnection FINAL : public ThreadSafeRefCounted<RemoteInspectorDebuggableConnection>, public InspectorFrontendChannel {
public:
    RemoteInspectorDebuggableConnection(RemoteInspectorDebuggable*, NSString *connectionIdentifier, NSString *destination, RemoteInspectorDebuggable::DebuggableType);
    virtual ~RemoteInspectorDebuggableConnection();

    NSString *destination() const;
    NSString *connectionIdentifier() const;
    unsigned identifier() const { return m_identifier; }

    bool setup();

    void close();
    void closeFromDebuggable();

    void sendMessageToBackend(NSString *);
    virtual bool sendMessageToFrontend(const String&) override;

private:
    void dispatchSyncOnDebuggable(void (^block)());
    void dispatchAsyncOnDebuggable(void (^block)());

    // This connection from the RemoteInspector singleton to the Debuggable
    // can be used on multiple threads. So any access to the debuggable
    // itself must take this lock to ensure m_debuggable is valid.
    Mutex m_debuggableLock;

    RemoteInspectorDebuggable* m_debuggable;
    dispatch_queue_t m_queueForDebuggable;
    RetainPtr<NSString> m_connectionIdentifier;
    RetainPtr<NSString> m_destination;
    unsigned m_identifier;
    bool m_connected;
};

} // namespace Inspector

#endif // RemoteInspectorDebuggableConnection_h

#endif // ENABLE(REMOTE_INSPECTOR)
