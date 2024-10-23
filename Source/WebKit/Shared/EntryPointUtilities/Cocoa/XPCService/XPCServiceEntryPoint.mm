/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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

#import "Logging.h"
#import "SandboxUtilities.h"
#import "XPCServiceEntryPoint.h"
#import <JavaScriptCore/JSCConfig.h>
#import <WebCore/ProcessIdentifier.h>
#import <signal.h>
#import <wtf/StdLibExtras.h>
#import <wtf/WTFProcess.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/StringToIntegerConversion.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebKit {

XPCServiceInitializerDelegate::~XPCServiceInitializerDelegate()
{
}

bool XPCServiceInitializerDelegate::checkEntitlements()
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (isClientSandboxed()) {
        // FIXME(<rdar://problem/54178641>): Remove this check once WebKit can work without network access.
        if (hasEntitlement("com.apple.security.network.client"_s))
            return true;

        audit_token_t auditToken = { };
        xpc_connection_get_audit_token(m_connection.get(), &auditToken);
        if (auto rc = sandbox_check_by_audit_token(auditToken, "mach-lookup", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_GLOBAL_NAME | SANDBOX_CHECK_NO_REPORT), "com.apple.nsurlsessiond")) {
            // FIXME (rdar://problem/54178641): This requirement is too strict, it should be possible to load file:// resources without network access.
            RELEASE_LOG_FAULT(Network, "Application does not have permission to communicate with network resources. rc=%d : errno=%d", rc, errno);
            return false;
        }
    }
#endif
    return true;
}

bool XPCServiceInitializerDelegate::getConnectionIdentifier(IPC::Connection::Identifier& identifier)
{
    mach_port_t port = xpc_dictionary_copy_mach_send(m_initializerMessage, "server-port");
    if (!MACH_PORT_VALID(port))
        return false;

    identifier = IPC::Connection::Identifier(port, m_connection);
    return true;
}

bool XPCServiceInitializerDelegate::getClientIdentifier(String& clientIdentifier)
{
    clientIdentifier = String::fromUTF8(xpc_dictionary_get_string(m_initializerMessage, "client-identifier"));
    return !clientIdentifier.isEmpty();
}

bool XPCServiceInitializerDelegate::getClientBundleIdentifier(String& clientBundleIdentifier)
{
    clientBundleIdentifier = String::fromLatin1(xpc_dictionary_get_string(m_initializerMessage, "client-bundle-identifier"));
    return !clientBundleIdentifier.isEmpty();
}

bool XPCServiceInitializerDelegate::getClientSDKAlignedBehaviors(SDKAlignedBehaviors& behaviors)
{
    auto behaviorData = xpc_dictionary_get_data_span(m_initializerMessage, "client-sdk-aligned-behaviors");
    if (behaviorData.empty())
        return false;
    memcpySpan(behaviors.storageBytes(), behaviorData);
    return true;
}

bool XPCServiceInitializerDelegate::getProcessIdentifier(std::optional<WebCore::ProcessIdentifier>& identifier)
{
    auto parsedIdentifier = parseInteger<uint64_t>(StringView::fromLatin1(xpc_dictionary_get_string(m_initializerMessage, "process-identifier")));
    if (!parsedIdentifier)
        return false;
    if (!ObjectIdentifier<WebCore::ProcessIdentifierType>::isValidIdentifier(*parsedIdentifier))
        return false;

    identifier = ObjectIdentifier<WebCore::ProcessIdentifierType>(*parsedIdentifier);
    return true;
}

bool XPCServiceInitializerDelegate::getClientProcessName(String& clientProcessName)
{
    clientProcessName = String::fromUTF8(xpc_dictionary_get_string(m_initializerMessage, "ui-process-name"));
    return !clientProcessName.isEmpty();
}

bool XPCServiceInitializerDelegate::getExtraInitializationData(HashMap<String, String>& extraInitializationData)
{
    xpc_object_t extraDataInitializationDataObject = xpc_dictionary_get_value(m_initializerMessage, "extra-initialization-data");

    auto inspectorProcess = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "inspector-process"));
    if (!inspectorProcess.isEmpty())
        extraInitializationData.add("inspector-process"_s, inspectorProcess);

    auto serviceWorkerProcess = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "service-worker-process"));
    if (!serviceWorkerProcess.isEmpty())
        extraInitializationData.add("service-worker-process"_s, WTFMove(serviceWorkerProcess));
    auto registrableDomain = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "registrable-domain"));
    if (!registrableDomain.isEmpty())
        extraInitializationData.add("registrable-domain"_s, WTFMove(registrableDomain));

    auto isPrewarmedProcess = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "is-prewarmed"));
    if (!isPrewarmedProcess.isEmpty())
        extraInitializationData.add("is-prewarmed"_s, isPrewarmedProcess);

    auto isLockdownModeEnabled = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "enable-lockdown-mode"));
    if (!isLockdownModeEnabled.isEmpty())
        extraInitializationData.add("enable-lockdown-mode"_s, isLockdownModeEnabled);

    if (!isClientSandboxed()) {
        auto userDirectorySuffix = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "user-directory-suffix"));
        if (!userDirectorySuffix.isEmpty())
            extraInitializationData.add("user-directory-suffix"_s, userDirectorySuffix);
    }

    auto alwaysRunsAtBackgroundPriority = String::fromLatin1(xpc_dictionary_get_string(extraDataInitializationDataObject, "always-runs-at-background-priority"));
    if (!alwaysRunsAtBackgroundPriority.isEmpty())
        extraInitializationData.add("always-runs-at-background-priority"_s, alwaysRunsAtBackgroundPriority);

    return true;
}

bool XPCServiceInitializerDelegate::hasEntitlement(ASCIILiteral entitlement)
{
    return WTF::hasEntitlement(m_connection.get(), entitlement);
}

bool XPCServiceInitializerDelegate::isClientSandboxed()
{
    return connectedProcessIsSandboxed(m_connection.get());
}

#if !USE(RUNNINGBOARD)
void setOSTransaction(OSObjectPtr<os_transaction_t>&& transaction)
{
    static NeverDestroyed<OSObjectPtr<os_transaction_t>> globalTransaction;
    static NeverDestroyed<OSObjectPtr<dispatch_source_t>> globalSource;

    // Because we don't use RunningBoard on macOS, we leak an OS transaction to control the lifetime of our XPC
    // services ourselves. However, one of the side effects of leaking this transaction is that the default SIGTERM
    // handler doesn't cleanly exit our XPC services when logging out or rebooting. This led to crashes with
    // XPC_EXIT_REASON_SIGTERM_TIMEOUT as termination reason (rdar://88940229). To address the issue, we now set our
    // own SIGTERM handler that calls exitProcess(0). In the future, we should likely adopt RunningBoard on macOS and
    // control our lifetime via process assertions instead of leaking this OS transaction.
    static dispatch_once_t flag;
    dispatch_once(&flag, ^{
        globalSource.get() = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue()));
        dispatch_source_set_event_handler(globalSource.get().get(), ^{
            exitProcess(0);
        });
        dispatch_resume(globalSource.get().get());
    });

    globalTransaction.get() = WTFMove(transaction);
}
#endif

void setJSCOptions(xpc_object_t initializerMessage, EnableLockdownMode enableLockdownMode, bool isWebContentProcess)
{
    RELEASE_ASSERT(!g_jscConfig.initializeHasBeenCalled);

    bool optionsChanged = false;
    if (xpc_dictionary_get_bool(initializerMessage, "configure-jsc-for-testing"))
        JSC::Config::configureForTesting();
    if (enableLockdownMode == EnableLockdownMode::Yes) {
        JSC::Options::machExceptionHandlerSandboxPolicy = JSC::Options::SandboxPolicy::Block;
        JSC::Options::initialize();
        JSC::Options::AllowUnfinalizedAccessScope scope;
        JSC::ExecutableAllocator::disableJIT();
        JSC::Options::useGenerationalGC() = false;
        JSC::Options::useConcurrentGC() = false;
        JSC::Options::useLLIntICs() = false;
        JSC::Options::useWasm() = false;
        JSC::Options::useZombieMode() = true;
        JSC::Options::allowDoubleShape() = false;
        JSC::Options::alwaysHaveABadTime() = true;
        optionsChanged = true;
    } else if (xpc_dictionary_get_bool(initializerMessage, "disable-jit")) {
        JSC::Options::initialize();
        JSC::Options::AllowUnfinalizedAccessScope scope;
        JSC::ExecutableAllocator::disableJIT();
        optionsChanged = true;
    }
    if (xpc_dictionary_get_bool(initializerMessage, "enable-shared-array-buffer")) {
        JSC::Options::initialize();
        JSC::Options::AllowUnfinalizedAccessScope scope;
        JSC::Options::useSharedArrayBuffer() = true;
        optionsChanged = true;
    }
    // FIXME (276012): Remove this XPC bootstrap message when it's no longer necessary. See rdar://130669638 for more context.
    if (xpc_dictionary_get_bool(initializerMessage, "disable-jit-cage")) {
        JSC::Options::initialize();
        JSC::Options::AllowUnfinalizedAccessScope scope;
        JSC::Options::useJITCage() = false;
        optionsChanged = true;
    }
    if (optionsChanged)
        JSC::Options::notifyOptionsChanged();
}

void XPCServiceExit()
{
#if !USE(RUNNINGBOARD)
    setOSTransaction(nullptr);
#endif

    xpc_transaction_exit_clean();
}

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
