/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PCMDaemonConnectionSet.h"

#import "PrivateClickMeasurementConnection.h"
#import "PrivateClickMeasurementManagerInterface.h"
#import <wtf/OSObjectPtr.h>
#import <wtf/RunLoop.h>

namespace WebKit::PCM {

DaemonConnectionSet& DaemonConnectionSet::singleton()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<DaemonConnectionSet> set;
    return set.get();
}
    
void DaemonConnectionSet::add(xpc_connection_t connection)
{
    ASSERT(!m_connections.contains(connection));
    m_connections.add(connection, DebugModeEnabled::No);
}

void DaemonConnectionSet::remove(xpc_connection_t connection)
{
    ASSERT(m_connections.contains(connection));
    auto hadDebugModeEnabled = m_connections.take(connection);
    if (hadDebugModeEnabled == DebugModeEnabled::Yes) {
        ASSERT(m_connectionsWithDebugModeEnabled);
        m_connectionsWithDebugModeEnabled--;
    }
}

void DaemonConnectionSet::setConnectedNetworkProcessHasDebugModeEnabled(const Daemon::Connection& connection, bool enabled)
{
    auto iterator = m_connections.find(connection.get());
    if (iterator == m_connections.end()) {
        ASSERT_NOT_REACHED();
        return;
    }
    bool wasEnabled = iterator->value == DebugModeEnabled::Yes;
    if (wasEnabled == enabled)
        return;

    if (enabled) {
        iterator->value = DebugModeEnabled::Yes;
        m_connectionsWithDebugModeEnabled++;
    } else {
        iterator->value = DebugModeEnabled::No;
        m_connectionsWithDebugModeEnabled--;
    }
}

bool DaemonConnectionSet::debugModeEnabled() const
{
    return !!m_connectionsWithDebugModeEnabled;
}

void DaemonConnectionSet::broadcastConsoleMessage(JSC::MessageLevel messageLevel, const String& message)
{
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(dictionary.get(), protocolDebugMessageLevelKey, static_cast<uint64_t>(messageLevel));
    xpc_dictionary_set_string(dictionary.get(), protocolDebugMessageKey, message.utf8().data());
    for (auto& connection : m_connections.keys())
        xpc_connection_send_message(connection.get(), dictionary.get());
}

} // namespace WebKit::PCM
