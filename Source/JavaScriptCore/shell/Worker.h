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

#pragma once

#include "JSCJSValue.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/URL.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class CallFrame;
class JSFunction;
class JSGlobalObject;
class VM;
}

namespace JSCShell {

class WorkerChannel;

class Worker {
public:
    // Returns as soon as the thread exists; the caller does not wait for body to run.
    static bool start(ASCIILiteral threadName, WTF::Function<void()>&& body);
};

struct InlineSource {
    String text;
};

struct ScriptPath {
    // Relative to WorkerScript::callerURL.
    String path;
};

struct WorkerScript {
    Variant<InlineSource, ScriptPath> script;
    URL callerURL;
    bool isModule { false };
};

JSC::JSFunction* createWorkerConstructor(JSC::VM&, JSC::JSGlobalObject*);

JSC_DECLARE_HOST_FUNCTION(functionWorkerScopePostMessage);
JSC_DECLARE_HOST_FUNCTION(functionWorkerScopeClose);

// FIXME: Move into Worker.cpp once runJSC, CommandLine, and GlobalObject are split out of jsc.cpp.
void startWorkerThread(JSC::JSGlobalObject*, WorkerScript&&, Ref<WorkerChannel>&&);

} // namespace JSCShell
