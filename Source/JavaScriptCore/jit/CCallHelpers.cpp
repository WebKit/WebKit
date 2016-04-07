/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CCallHelpers.h"

#if ENABLE(JIT)

#include "ShadowChicken.h"

namespace JSC {

void CCallHelpers::logShadowChickenProloguePacket()
{
    setupShadowChickenPacket();
    storePtr(GPRInfo::callFrameRegister, Address(GPRInfo::regT1, OBJECT_OFFSETOF(ShadowChicken::Packet, frame)));
    loadPtr(Address(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(CallerFrameAndPC, callerFrame)), GPRInfo::regT0);
    storePtr(GPRInfo::regT0, Address(GPRInfo::regT1, OBJECT_OFFSETOF(ShadowChicken::Packet, callerFrame)));
    loadPtr(addressFor(JSStack::Callee), GPRInfo::regT0);
    storePtr(GPRInfo::regT0, Address(GPRInfo::regT1, OBJECT_OFFSETOF(ShadowChicken::Packet, callee)));
}

void CCallHelpers::logShadowChickenTailPacket()
{
    setupShadowChickenPacket();
    storePtr(GPRInfo::callFrameRegister, Address(GPRInfo::regT1, OBJECT_OFFSETOF(ShadowChicken::Packet, frame)));
    storePtr(TrustedImmPtr(ShadowChicken::Packet::tailMarker()), Address(GPRInfo::regT1, OBJECT_OFFSETOF(ShadowChicken::Packet, callee)));
}

void CCallHelpers::setupShadowChickenPacket()
{
    move(TrustedImmPtr(vm()->shadowChicken().addressOfLogCursor()), GPRInfo::regT0);
    loadPtr(Address(GPRInfo::regT0), GPRInfo::regT1);
    Jump ok = branchPtr(Below, GPRInfo::regT1, TrustedImmPtr(vm()->shadowChicken().logEnd()));
    setupArgumentsExecState();
    move(TrustedImmPtr(bitwise_cast<void*>(operationProcessShadowChickenLog)), GPRInfo::nonArgGPR0);
    call(GPRInfo::nonArgGPR0);
    move(TrustedImmPtr(vm()->shadowChicken().addressOfLogCursor()), GPRInfo::regT0);
    loadPtr(Address(GPRInfo::regT0), GPRInfo::regT1);
    ok.link(this);
    addPtr(TrustedImm32(sizeof(ShadowChicken::Packet)), GPRInfo::regT1, GPRInfo::regT2);
    storePtr(GPRInfo::regT2, Address(GPRInfo::regT0));
}

} // namespace JSC

#endif // ENABLE(JIT)
