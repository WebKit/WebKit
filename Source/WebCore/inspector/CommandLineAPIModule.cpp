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
#include "CommandLineAPIModule.h"

#include "CommandLineAPIModuleSource.h"
#include "JSDOMGlobalObject.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/InjectedScript.h>

namespace WebCore {

using namespace JSC;
using namespace Inspector;

void CommandLineAPIModule::injectIfNeeded(InjectedScriptManager* injectedScriptManager, const InjectedScript& injectedScript)
{
    CommandLineAPIModule module;
    module.ensureInjected(injectedScriptManager, injectedScript);
}

CommandLineAPIModule::CommandLineAPIModule()
    : InjectedScriptModule("CommandLineAPI"_s)
{
}

String CommandLineAPIModule::source() const
{
    return StringImpl::createWithoutCopying(CommandLineAPIModuleSource_js, sizeof(CommandLineAPIModuleSource_js));
}

JSValue CommandLineAPIModule::host(InjectedScriptManager* injectedScriptManager, ExecState* exec) const
{
    // CommandLineAPIModule should only ever be used by a WebInjectedScriptManager.
    WebInjectedScriptManager* pageInjectedScriptManager = static_cast<WebInjectedScriptManager*>(injectedScriptManager);
    ASSERT(pageInjectedScriptManager->commandLineAPIHost());

    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
    return pageInjectedScriptManager->commandLineAPIHost()->wrapper(exec, globalObject);
}

} // namespace WebCore
