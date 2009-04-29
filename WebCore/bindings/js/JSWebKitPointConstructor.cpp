/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "JSWebKitPointConstructor.h"

#include "Document.h"
#include "WebKitPoint.h"
#include "JSWebKitPoint.h"

namespace WebCore {

using namespace JSC;

const ClassInfo JSWebKitPointConstructor::s_info = { "WebKitPointConstructor", 0, 0, 0 };

JSWebKitPointConstructor::JSWebKitPointConstructor(ExecState* exec)
    : DOMObject(JSWebKitPointConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
{
    putDirect(exec->propertyNames().prototype, JSWebKitPointPrototype::self(exec, exec->lexicalGlobalObject()), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 2), ReadOnly|DontDelete|DontEnum);
}

static JSObject* constructWebKitPoint(ExecState* exec, JSObject*, const ArgList& args)
{
    float x = 0;
    float y = 0;
    if (args.size() >= 2) {
        x = (float)args.at(0).toNumber(exec);
        y = (float)args.at(1).toNumber(exec);
        if (isnan(x))
            x = 0;
        if (isnan(y))
            y = 0;
    }
    return asObject(toJS(exec, WebKitPoint::create(x, y)));
}

JSC::ConstructType JSWebKitPointConstructor::getConstructData(JSC::ConstructData& constructData)
{
    constructData.native.function = constructWebKitPoint;
    return ConstructTypeHost;
}


} // namespace WebCore
