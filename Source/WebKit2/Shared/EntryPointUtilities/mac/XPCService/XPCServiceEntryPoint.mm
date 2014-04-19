/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "XPCServiceEntryPoint.h"

#if __has_include(<xpc/private.h>)
#import <xpc/private.h>
#else
extern "C" xpc_object_t xpc_connection_copy_entitlement_value(xpc_connection_t connection, const char *entitlement);
extern "C" mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t, const char*);
#endif

#if __has_include(<sandbox/private.h>)
#import <sandbox/private.h>
#else
enum sandbox_filter_type {
    SANDBOX_FILTER_NONE,
};
extern "C"
int sandbox_check(pid_t, const char *operation, enum sandbox_filter_type, ...);
#endif

namespace WebKit {

XPCServiceInitializerDelegate::~XPCServiceInitializerDelegate()
{
}

bool XPCServiceInitializerDelegate::checkEntitlements()
{
    return true;
}

bool XPCServiceInitializerDelegate::getConnectionIdentifier(IPC::Connection::Identifier& identifier)
{
    identifier = IPC::Connection::Identifier(xpc_dictionary_copy_mach_send(m_initializerMessage, "server-port"), m_connection);
    return true;
}

bool XPCServiceInitializerDelegate::getClientIdentifier(String& clientIdentifier)
{
    clientIdentifier = xpc_dictionary_get_string(m_initializerMessage, "client-identifier");
    if (clientIdentifier.isEmpty())
        return false;
    return true;
}

bool XPCServiceInitializerDelegate::getClientProcessName(String& clientProcessName)
{
    clientProcessName = xpc_dictionary_get_string(m_initializerMessage, "ui-process-name");
    if (clientProcessName.isEmpty())
        return false;
    return true;
}

bool XPCServiceInitializerDelegate::getExtraInitializationData(HashMap<String, String>&)
{
    return true;
}

bool XPCServiceInitializerDelegate::hasEntitlement(const char* entitlement)
{
    xpc_object_t value = xpc_connection_copy_entitlement_value(m_connection, entitlement);
    if (!value)
        return false;

    bool result = xpc_get_type(value) == XPC_TYPE_BOOL && xpc_bool_get_value(value);

    xpc_release(value);

    return result;
}

bool XPCServiceInitializerDelegate::isClientSandboxed()
{
    pid_t clientPID = xpc_connection_get_pid(m_connection);

    return sandbox_check(clientPID, nullptr, SANDBOX_FILTER_NONE);
}

} // namespace WebKit
