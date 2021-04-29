/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

namespace WebKit {

class GPUServiceInitializerDelegate : public XPCServiceInitializerDelegate {
public:
    GPUServiceInitializerDelegate(OSObjectPtr<xpc_connection_t> connection, xpc_object_t initializerMessage)
        : XPCServiceInitializerDelegate(WTFMove(connection), initializerMessage)
    {
    }
};

template<>
void initializeAuxiliaryProcess<GPUProcess>(AuxiliaryProcessInitializationParameters&& parameters)
{
    static NeverDestroyed<GPUProcess> gpuProcess(WTFMove(parameters));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)

extern "C" WK_EXPORT void GPU_SERVICE_INITIALIZER(xpc_connection_t connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage);

void GPU_SERVICE_INITIALIZER(xpc_connection_t connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage)
{
    WTF::initializeMainThread();

#if ENABLE(GPU_PROCESS)
    WebKit::XPCServiceInitializer<WebKit::GPUProcess, WebKit::GPUServiceInitializerDelegate>(connection, initializerMessage, priorityBoostMessage);
#endif // ENABLE(GPU_PROCESS)
}
