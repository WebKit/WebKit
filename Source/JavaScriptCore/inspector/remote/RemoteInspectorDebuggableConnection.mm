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

#import "config.h"
#import "RemoteInspectorDebuggableConnection.h"

#if ENABLE(REMOTE_INSPECTOR)

#import "RemoteInspector.h"

#if PLATFORM(IOS)
#import <wtf/ios/WebCoreThread.h>
#endif

namespace Inspector {

RemoteInspectorDebuggableConnection::RemoteInspectorDebuggableConnection(RemoteInspectorDebuggable* debuggable, NSString *connectionIdentifier, NSString *destination, RemoteInspectorDebuggable::DebuggableType type)
    : m_debuggable(debuggable)
    , m_queueForDebuggable(NULL)
    , m_connectionIdentifier(connectionIdentifier)
    , m_destination(destination)
    , m_identifier(debuggable->identifier())
    , m_connected(false)
{
    // Web debuggables must be accessed on the main queue (or the WebThread on iOS). Signal that with a NULL m_queueForDebuggable.
    // However, JavaScript debuggables can be accessed from any thread/queue, so we create a queue for each JavaScript debuggable.
    if (type == RemoteInspectorDebuggable::JavaScript)
        m_queueForDebuggable = dispatch_queue_create("com.apple.JavaScriptCore.remote-inspector-debuggable-connection", DISPATCH_QUEUE_SERIAL);
}

RemoteInspectorDebuggableConnection::~RemoteInspectorDebuggableConnection()
{
    if (m_queueForDebuggable) {
        dispatch_release(m_queueForDebuggable);
        m_queueForDebuggable = NULL;
    }
}

NSString *RemoteInspectorDebuggableConnection::destination() const
{
    return [[m_destination copy] autorelease];
}

NSString *RemoteInspectorDebuggableConnection::connectionIdentifier() const
{
    return [[m_connectionIdentifier copy] autorelease];
}

void RemoteInspectorDebuggableConnection::dispatchAsyncOnDebuggable(void (^block)())
{
    if (m_queueForDebuggable)
        dispatch_async(m_queueForDebuggable, block);
#if PLATFORM(IOS)
    else if (WebCoreWebThreadIsEnabled && WebCoreWebThreadIsEnabled())
        WebCoreWebThreadRun(block);
#endif
    else
        dispatch_async(dispatch_get_main_queue(), block);
}

bool RemoteInspectorDebuggableConnection::setup()
{
    std::lock_guard<std::mutex> lock(m_debuggableMutex);

    if (!m_debuggable)
        return false;

    ref();
    dispatchAsyncOnDebuggable(^{
        {
            std::lock_guard<std::mutex> lock(m_debuggableMutex);
            if (!m_debuggable || !m_debuggable->remoteDebuggingAllowed() || m_debuggable->hasLocalDebugger()) {
                RemoteInspector::shared().setupFailed(identifier());
                m_debuggable = nullptr;
            } else {
                m_debuggable->connect(this);
                m_connected = true;
            }
        }
        deref();
    });

    return true;
}

void RemoteInspectorDebuggableConnection::closeFromDebuggable()
{
    std::lock_guard<std::mutex> lock(m_debuggableMutex);

    m_debuggable = nullptr;
}

void RemoteInspectorDebuggableConnection::close()
{
    ref();
    dispatchAsyncOnDebuggable(^{
        {
            std::lock_guard<std::mutex> lock(m_debuggableMutex);

            if (m_debuggable) {
                if (m_connected)
                    m_debuggable->disconnect();

                m_debuggable = nullptr;
            }
        }
        deref();
    });
}

void RemoteInspectorDebuggableConnection::sendMessageToBackend(NSString *message)
{
    ref();
    dispatchAsyncOnDebuggable(^{
        {
            std::lock_guard<std::mutex> lock(m_debuggableMutex);

            if (m_debuggable)
                m_debuggable->dispatchMessageFromRemoteFrontend(message);
        }
        deref();
    });
}

bool RemoteInspectorDebuggableConnection::sendMessageToFrontend(const String& message)
{
    RemoteInspector::shared().sendMessageToRemoteFrontend(identifier(), message);

    return true;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
