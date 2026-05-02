/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/Options.h>
#include <JavaScriptCore/VMManager.h>
#include <JavaScriptCore/VMTraps.h>
#include <wtf/Seconds.h>
#include <wtf/threads/BinarySemaphore.h>

// Worker threads that block in a WebCore operation while the VM is entered
// (actively executing JS, vm.isEntered()==true) prevent the WASM debugger's
// Stop-The-World (STW) protocol from completing: the VM never reaches a JSC
// trap check point to call notifyVMStop().
// Use waitWithSTWParticipation() instead of semaphore.wait()
// at any such site so the VM cooperatively participates in STW while blocked.

namespace WebCore {

inline void waitWithSTWParticipation(WTF::BinarySemaphore& semaphore, JSC::VM& vm)
{
#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (JSC::Options::enableWasmDebugger()) [[unlikely]] {
        while (!semaphore.waitFor(JSC::kDebuggerSTWCheckInterval)) {
            if (vm.traps().hasTrapBit(JSC::VMTraps::NeedStopTheWorld))
                JSC::VMManager::singleton().notifyVMStop(vm, JSC::StopTheWorldEvent::VMStopped);
        }
        return;
    }
#else
    UNUSED_PARAM(vm);
#endif
    semaphore.wait();
}

} // namespace WebCore
