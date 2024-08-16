/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ProcessTerminationReason.h"
#include <wtf/text/ASCIILiteral.h>

namespace WebKit {

ASCIILiteral processTerminationReasonToString(ProcessTerminationReason reason)
{
    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
        return "ExceededMemoryLimit"_s;
    case ProcessTerminationReason::ExceededCPULimit:
        return "ExceededCPULimit"_s;
    case ProcessTerminationReason::RequestedByClient:
        return "RequestedByClient"_s;
    case ProcessTerminationReason::IdleExit:
        return "IdleExit"_s;
    case ProcessTerminationReason::Unresponsive:
        return "Unresponsive"_s;
    case ProcessTerminationReason::Crash:
        return "Crash"_s;
    case ProcessTerminationReason::ExceededProcessCountLimit:
        return "ExceededProcessCountLimit"_s;
    case ProcessTerminationReason::NavigationSwap:
        return "NavigationSwap"_s;
    case ProcessTerminationReason::RequestedByNetworkProcess:
        return "RequestedByNetworkProcess"_s;
    case ProcessTerminationReason::RequestedByGPUProcess:
        return "RequestedByGPUProcess"_s;
    case ProcessTerminationReason::RequestedByModelProcess:
        return "RequestedByModelProcess"_s;
    case ProcessTerminationReason::GPUProcessCrashedTooManyTimes:
        return "GPUProcessCrashedTooManyTimes"_s;
    case ProcessTerminationReason::ModelProcessCrashedTooManyTimes:
        return "ModelProcessCrashedTooManyTimes"_s;
    }

    return ""_s;
}

}
