/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <optional>
#include <wtf/Forward.h>
#include <wtf/ProcessID.h>

namespace WTF {

WTF_EXPORT_PRIVATE void setLegacyPresentingApplicationPID(int);
WTF_EXPORT_PRIVATE int legacyPresentingApplicationPID();

#if HAVE(AUDIT_TOKEN)
WTF_EXPORT_PRIVATE ProcessID pidFromAuditToken(const audit_token_t&);
#endif

enum class AuxiliaryProcessType : uint8_t {
    WebContent,
    Network,
    Plugin,
#if ENABLE(GPU_PROCESS)
    GPU,
#endif
#if ENABLE(MODEL_PROCESS)
    Model,
#endif
};

WTF_EXPORT_PRIVATE void setAuxiliaryProcessType(AuxiliaryProcessType);
WTF_EXPORT_PRIVATE void setAuxiliaryProcessTypeForTesting(std::optional<AuxiliaryProcessType>);
WTF_EXPORT_PRIVATE bool checkAuxiliaryProcessType(AuxiliaryProcessType);
WTF_EXPORT_PRIVATE std::optional<AuxiliaryProcessType> processType();
WTF_EXPORT_PRIVATE ASCIILiteral processTypeDescription(std::optional<AuxiliaryProcessType>);

WTF_EXPORT_PRIVATE bool isInAuxiliaryProcess();
inline bool isInWebProcess() { return checkAuxiliaryProcessType(AuxiliaryProcessType::WebContent); }
inline bool isInNetworkProcess() { return checkAuxiliaryProcessType(AuxiliaryProcessType::Network); }
inline bool isInGPUProcess()
{
#if ENABLE(GPU_PROCESS)
    return checkAuxiliaryProcessType(AuxiliaryProcessType::GPU);
#else
    return false;
#endif
}

inline bool isInModelProcess()
{
#if ENABLE(MODEL_PROCESS)
    return checkAuxiliaryProcessType(AuxiliaryProcessType::Model);
#else
    return false;
#endif
}

} // namespace WTF

using WTF::checkAuxiliaryProcessType;
using WTF::isInAuxiliaryProcess;
using WTF::isInGPUProcess;
using WTF::isInModelProcess;
using WTF::isInNetworkProcess;
using WTF::isInWebProcess;
using WTF::legacyPresentingApplicationPID;
using WTF::processType;
using WTF::processTypeDescription;
using WTF::setAuxiliaryProcessType;
using WTF::setAuxiliaryProcessTypeForTesting;
using WTF::setLegacyPresentingApplicationPID;

#if HAVE(AUDIT_TOKEN)
using WTF::pidFromAuditToken;
#endif
