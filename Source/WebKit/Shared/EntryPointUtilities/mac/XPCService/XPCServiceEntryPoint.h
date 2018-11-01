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

#ifndef XPCServiceEntryPoint_h
#define XPCServiceEntryPoint_h

#import "ChildProcess.h"
#import "WebKit2Initialize.h"
#import <wtf/OSObjectPtr.h>
#import <wtf/spi/darwin/XPCSPI.h>

#if HAVE(VOUCHERS)
#if USE(APPLE_INTERNAL_SDK)
#include <os/voucher_private.h>
#else
extern "C" OS_NOTHROW void voucher_replace_default_voucher(void);
#endif
#endif

namespace WebKit {

class XPCServiceInitializerDelegate {
public:
    XPCServiceInitializerDelegate(OSObjectPtr<xpc_connection_t> connection, xpc_object_t initializerMessage)
        : m_connection(WTFMove(connection))
        , m_initializerMessage(initializerMessage)
    {
    }

    virtual ~XPCServiceInitializerDelegate();

    virtual bool checkEntitlements();

    virtual bool getConnectionIdentifier(IPC::Connection::Identifier& identifier);
    virtual bool getProcessIdentifier(WebCore::ProcessIdentifier&);
    virtual bool getClientIdentifier(String& clientIdentifier);
    virtual bool getClientProcessName(String& clientProcessName);
    virtual bool getExtraInitializationData(HashMap<String, String>& extraInitializationData);

protected:
    bool hasEntitlement(const char* entitlement);
    bool isClientSandboxed();

    OSObjectPtr<xpc_connection_t> m_connection;
    xpc_object_t m_initializerMessage;
};

template<typename XPCServiceType, typename XPCServiceInitializerDelegateType>
void XPCServiceInitializer(OSObjectPtr<xpc_connection_t> connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage)
{
    XPCServiceInitializerDelegateType delegate(WTFMove(connection), initializerMessage);

    // We don't want XPC to be in charge of whether the process should be terminated or not,
    // so ensure that we have an outstanding transaction here.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    xpc_transaction_begin();
ALLOW_DEPRECATED_DECLARATIONS_END

    InitializeWebKit2();

    if (!delegate.checkEntitlements())
        exit(EXIT_FAILURE);

    ChildProcessInitializationParameters parameters;
    if (priorityBoostMessage)
        parameters.priorityBoostMessage = priorityBoostMessage;

    if (!delegate.getConnectionIdentifier(parameters.connectionIdentifier))
        exit(EXIT_FAILURE);

    if (!delegate.getClientIdentifier(parameters.clientIdentifier))
        exit(EXIT_FAILURE);

    WebCore::ProcessIdentifier processIdentifier;
    if (!delegate.getProcessIdentifier(processIdentifier))
        exit(EXIT_FAILURE);
    parameters.processIdentifier = processIdentifier;

    if (!delegate.getClientProcessName(parameters.uiProcessName))
        exit(EXIT_FAILURE);

    if (!delegate.getExtraInitializationData(parameters.extraInitializationData))
        exit(EXIT_FAILURE);

#if HAVE(VOUCHERS)
    // Set the task default voucher to the current value (as propagated by XPC).
    voucher_replace_default_voucher();
#endif

#if HAVE(QOS_CLASSES)
    if (parameters.extraInitializationData.contains("always-runs-at-background-priority"_s))
        Thread::setGlobalMaxQOSClass(QOS_CLASS_UTILITY);
#endif

    parameters.processType = XPCServiceType::processType;

    XPCServiceType::singleton().initialize(parameters);
}

int XPCServiceMain(int, const char**);

void XPCServiceExit(OSObjectPtr<xpc_object_t>&& priorityBoostMessage);

} // namespace WebKit

#endif // XPCServiceEntryPoint_h
