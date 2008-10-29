/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "JSDedicatedWorkerConstructor.h"

#include "DedicatedWorker.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "JSDOMWindowCustom.h"
#include "JSDedicatedWorker.h"

using namespace JSC;

namespace WebCore {

const ClassInfo JSDedicatedWorkerConstructor::s_info = { "DedicatedWorkerConstructor", 0, 0, 0 };

JSDedicatedWorkerConstructor::JSDedicatedWorkerConstructor(ExecState* exec)
    : DOMObject(JSDedicatedWorkerConstructor::createStructureID(exec->lexicalGlobalObject()->objectPrototype()))
{
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly|DontDelete|DontEnum);
}

static JSObject* constructDedicatedWorker(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    if (args.size() == 0)
        return throwError(exec, SyntaxError, "Not enough arguments");

    UString scriptURL = args.at(exec, 0)->toString(exec);

    DOMWindow* window = asJSDOMWindow(exec->lexicalGlobalObject())->impl();
    
    ExceptionCode ec = 0;
    RefPtr<DedicatedWorker> worker = DedicatedWorker::create(scriptURL, window->document(), ec);
    setDOMException(exec, ec);

    return asObject(toJS(exec, worker.release()));
}

ConstructType JSDedicatedWorkerConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructDedicatedWorker;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
