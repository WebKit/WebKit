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
#include "CommonVM.h"

#include "ScriptController.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include <heap/HeapInlines.h>
#include "heap/MachineStackMarker.h"
#include <runtime/VM.h>
#include <wtf/MainThread.h>
#include <wtf/text/AtomicString.h>

#if PLATFORM(IOS)
#include "WebCoreThreadInternal.h"
#endif

using namespace JSC;

namespace WebCore {

VM* g_commonVMOrNull;

VM& commonVMSlow()
{
    ASSERT(isMainThread());
    ASSERT(!g_commonVMOrNull);
    
    ScriptController::initializeThreading();
    g_commonVMOrNull = &VM::createLeaked(LargeHeap).leakRef();
    g_commonVMOrNull->heap.acquireAccess(); // At any time, we may do things that affect the GC.
#if !PLATFORM(IOS)
    g_commonVMOrNull->setExclusiveThread(std::this_thread::get_id());
#else
    g_commonVMOrNull->heap.setRunLoop(WebThreadRunLoop());
    g_commonVMOrNull->heap.machineThreads().addCurrentThread();
#endif
    
    g_commonVMOrNull->setGlobalConstRedeclarationShouldThrow(Settings::globalConstRedeclarationShouldThrow());
    
    JSVMClientData::initNormalWorld(g_commonVMOrNull);
    
    return *g_commonVMOrNull;
}

void addImpureProperty(const AtomicString& propertyName)
{
    commonVM().addImpureProperty(propertyName);
}

} // namespace WebCore

