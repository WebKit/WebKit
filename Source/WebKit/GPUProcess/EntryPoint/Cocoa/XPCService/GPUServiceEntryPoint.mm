/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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

#import "EnvironmentUtilities.h"
#import "GPUProcess.h"
#import "WKBase.h"
#import "XPCServiceEntryPoint.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(GPU_PROCESS)

namespace WebKit {

class GPUServiceInitializerDelegate : public XPCServiceInitializerDelegate {
public:
    GPUServiceInitializerDelegate(OSObjectPtr<xpc_connection_t> connection, xpc_object_t initializerMessage)
        : XPCServiceInitializerDelegate(WTFMove(connection), initializerMessage)
    {
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

extern "C" WK_EXPORT void GPU_SERVICE_INITIALIZER(xpc_connection_t connection, xpc_object_t initializerMessage);

void GPU_SERVICE_INITIALIZER(xpc_connection_t connection, xpc_object_t initializerMessage)
{
    g_jscConfig.vmCreationDisallowed = true;
    g_jscConfig.vmEntryDisallowed = true;
    g_wtfConfig.useSpecialAbortForExtraSecurityImplications = true;

    WTF::initializeMainThread();
    {
        JSC::Options::initialize();
        JSC::Options::AllowUnfinalizedAccessScope scope;
        JSC::ExecutableAllocator::disableJIT();
        JSC::Options::useWasm() = false;
        JSC::Options::notifyOptionsChanged();
    }
    WTF::compilerFence();

#if ENABLE(GPU_PROCESS)
    WebKit::XPCServiceInitializer<WebKit::GPUProcess, WebKit::GPUServiceInitializerDelegate>(connection, initializerMessage);
#endif // ENABLE(GPU_PROCESS)

    JSC::Config::finalize();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
