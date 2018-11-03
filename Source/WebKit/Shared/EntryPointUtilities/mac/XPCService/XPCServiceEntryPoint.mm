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

#import "ArgumentCodersCF.h"
#import "SandboxUtilities.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/Process.h>
#import <wtf/cocoa/Entitlements.h>

namespace WebKit {
using namespace WebCore;

XPCServiceInitializerDelegate::~XPCServiceInitializerDelegate()
{
}

bool XPCServiceInitializerDelegate::checkEntitlements()
{
#if PLATFORM(MAC)
    if (!isClientSandboxed())
        return true;

    // FIXME: Once we're 100% sure that a process can't access the network we can get rid of this requirement for all processes.
    if (!hasEntitlement("com.apple.security.network.client")) {
        NSLog(@"Application does not have the 'com.apple.security.network.client' entitlement.");
        return false;
    }
#endif
#if PLATFORM(IOS_FAMILY)
    auto value = adoptOSObject(xpc_connection_copy_entitlement_value(m_connection.get(), "keychain-access-groups"));
    if (value && xpc_get_type(value.get()) == XPC_TYPE_ARRAY) {
        xpc_array_apply(value.get(), ^bool(size_t index, xpc_object_t object) {
            if (xpc_get_type(object) == XPC_TYPE_STRING && !strcmp(xpc_string_get_string_ptr(object), "com.apple.identities")) {
                IPC::setAllowsDecodingSecKeyRef(true);
                return false;
            }
            return true;
        });
    }
#endif

    return true;
}

bool XPCServiceInitializerDelegate::getConnectionIdentifier(IPC::Connection::Identifier& identifier)
{
    mach_port_t port = xpc_dictionary_copy_mach_send(m_initializerMessage, "server-port");
    if (port == MACH_PORT_NULL)
        return false;

    identifier = IPC::Connection::Identifier(port, m_connection);
    return true;
}

bool XPCServiceInitializerDelegate::getClientIdentifier(String& clientIdentifier)
{
    clientIdentifier = xpc_dictionary_get_string(m_initializerMessage, "client-identifier");
    if (clientIdentifier.isEmpty())
        return false;
    return true;
}

bool XPCServiceInitializerDelegate::getProcessIdentifier(ProcessIdentifier& identifier)
{
    String processIdentifierString = xpc_dictionary_get_string(m_initializerMessage, "process-identifier");
    if (processIdentifierString.isEmpty())
        return false;

    bool ok;
    auto parsedIdentifier = processIdentifierString.toUInt64Strict(&ok);
    if (!ok)
        return false;

    identifier = makeObjectIdentifier<ProcessIdentifierType>(parsedIdentifier);
    return true;
}

bool XPCServiceInitializerDelegate::getClientProcessName(String& clientProcessName)
{
    clientProcessName = xpc_dictionary_get_string(m_initializerMessage, "ui-process-name");
    if (clientProcessName.isEmpty())
        return false;
    return true;
}

bool XPCServiceInitializerDelegate::getExtraInitializationData(HashMap<String, String>& extraInitializationData)
{
    xpc_object_t extraDataInitializationDataObject = xpc_dictionary_get_value(m_initializerMessage, "extra-initialization-data");

    String inspectorProcess = xpc_dictionary_get_string(extraDataInitializationDataObject, "inspector-process");
    if (!inspectorProcess.isEmpty())
        extraInitializationData.add("inspector-process"_s, inspectorProcess);

#if ENABLE(SERVICE_WORKER)
    String serviceWorkerProcess = xpc_dictionary_get_string(extraDataInitializationDataObject, "service-worker-process");
    if (!serviceWorkerProcess.isEmpty())
        extraInitializationData.add("service-worker-process"_s, serviceWorkerProcess);
#endif

    String securityOrigin = xpc_dictionary_get_string(extraDataInitializationDataObject, "security-origin");
    if (!securityOrigin.isEmpty())
        extraInitializationData.add("security-origin"_s, securityOrigin);

    if (!isClientSandboxed()) {
        String userDirectorySuffix = xpc_dictionary_get_string(extraDataInitializationDataObject, "user-directory-suffix");
        if (!userDirectorySuffix.isEmpty())
            extraInitializationData.add("user-directory-suffix"_s, userDirectorySuffix);
    }

    String alwaysRunsAtBackgroundPriority = xpc_dictionary_get_string(extraDataInitializationDataObject, "always-runs-at-background-priority");
    if (!alwaysRunsAtBackgroundPriority.isEmpty())
        extraInitializationData.add("always-runs-at-background-priority"_s, alwaysRunsAtBackgroundPriority);

    return true;
}

bool XPCServiceInitializerDelegate::hasEntitlement(const char* entitlement)
{
    return WTF::hasEntitlement(m_connection.get(), entitlement);
}

bool XPCServiceInitializerDelegate::isClientSandboxed()
{
    return connectedProcessIsSandboxed(m_connection.get());
}

void XPCServiceExit(OSObjectPtr<xpc_object_t>&& priorityBoostMessage)
{
    // Make sure to destroy the priority boost message to avoid leaking a transaction.
    priorityBoostMessage = nullptr;
    // Balances the xpc_transaction_begin() in XPCServiceInitializer.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    xpc_transaction_end();
ALLOW_DEPRECATED_DECLARATIONS_END
    xpc_transaction_exit_clean();
}

} // namespace WebKit
