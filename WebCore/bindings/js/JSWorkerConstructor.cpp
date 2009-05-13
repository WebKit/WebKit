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

#include "JSWorkerConstructor.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "JSDOMWindowCustom.h"
#include "JSWorker.h"
#include "Worker.h"

using namespace JSC;

namespace WebCore {

const ClassInfo JSWorkerConstructor::s_info = { "WorkerConstructor", 0, 0, 0 };

JSWorkerConstructor::JSWorkerConstructor(ExecState* exec)
    : DOMObject(JSWorkerConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
{
    putDirect(exec->propertyNames().prototype, JSWorkerPrototype::self(exec, exec->lexicalGlobalObject()), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly|DontDelete|DontEnum);
}

static JSObject* constructWorker(ExecState* exec, JSObject*, const ArgList& args)
{
    if (args.size() == 0)
        return throwError(exec, SyntaxError, "Not enough arguments");

    UString scriptURL = args.at(0).toString(exec);
    if (exec->hadException())
        return 0;

    DOMWindow* window = asJSDOMWindow(exec->lexicalGlobalObject())->impl();
    
    ExceptionCode ec = 0;
    RefPtr<Worker> worker = Worker::create(scriptURL, window->document(), ec);
    setDOMException(exec, ec);

    return asObject(toJS(exec, worker.release()));
}

ConstructType JSWorkerConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWorker;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
