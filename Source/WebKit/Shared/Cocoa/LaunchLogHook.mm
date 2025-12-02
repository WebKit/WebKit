/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "LaunchLogHook.h"

#import "LaunchLogMessages.h"
#import "Logging.h"
#import "XPCEndpoint.h"
#import <wtf/BlockPtr.h>
#import <wtf/SystemFree.h>
#import <wtf/spi/cocoa/OSLogSPI.h>

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

namespace WebKit {

LaunchLogHook& LaunchLogHook::singleton()
{
    static NeverDestroyed<LaunchLogHook> logHook;
    return logHook.get();
}

void LaunchLogHook::initialize(xpc_connection_t connection)
{
    {
        Locker locker { m_lock };
        m_connection = connection;
    }

    static os_log_hook_t prevHook = nullptr;

    auto blockPtr = makeBlockPtr([](os_log_type_t type, os_log_message_t msg) {
        if (prevHook)
            prevHook(type, msg);

        Locker locker { LaunchLogHook::singleton().m_lock };

        if (!LaunchLogHook::singleton().m_connection)
            return;

        if (type & OS_LOG_TYPE_DEBUG)
            return;

        if (type == OS_LOG_TYPE_FAULT)
            type = OS_LOG_TYPE_ERROR;

        if (auto messageString = adoptSystemMalloc(os_log_copy_message_string(msg))) {
            // FIXME: This is a false positive. <rdar://164843889>
            SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptXPCObject(xpc_dictionary_create(nullptr, nullptr, 0));
            xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, logMessageName);
            if (auto* subsystem = msg->subsystem)
                xpc_dictionary_set_string(message.get(), subsystemKey, subsystem);
            if (auto* category = msg->category)
                xpc_dictionary_set_string(message.get(), categoryKey, category);
            xpc_dictionary_set_string(message.get(), messageStringKey, messageString.get());
            xpc_dictionary_set_uint64(message.get(), logTypeKey, type);
            xpc_connection_send_message(LaunchLogHook::singleton().m_connection.get(), message.get());
        }
    });

    prevHook = os_log_set_hook(OS_LOG_TYPE_DEFAULT, blockPtr.get());
    RELEASE_LOG(Process, "Installed launch log hook");
}

void LaunchLogHook::disable()
{
    RELEASE_LOG(Process, "Disabling launch log hook");

    XPCObjectPtr<xpc_connection_t> connection;
    {
        Locker locker { m_lock };
        connection = m_connection;
        m_connection = nullptr;
    }
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptXPCObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, disableLogMessageName);
    xpc_connection_send_message(connection.get(), message.get());
}

} // namespace WebKit

#endif
