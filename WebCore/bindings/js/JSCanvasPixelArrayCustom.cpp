/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "JSCanvasPixelArray.h"

#include "CanvasPixelArray.h"

using namespace KJS;

namespace WebCore {

JSValue* JSCanvasPixelArray::indexGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    CanvasPixelArray* array = static_cast<JSCanvasPixelArray*>(slot.slotBase())->impl();
    unsigned index = slot.index();
    unsigned char result;
    if (!array->get(index, result))
        return jsUndefined();
    return jsNumber(exec, result);
}

void JSCanvasPixelArray::indexSetter(ExecState* exec, unsigned index, JSValue* value)
{
    double pixelValue = value->toNumber(exec);
    if (exec->hadException())
        return;
    m_impl->set(index, pixelValue); 
}

JSValue* toJS(ExecState* exec, CanvasPixelArray* pixels)
{
    if (!pixels)
        return jsNull();
    
    DOMObject* ret = ScriptInterpreter::getDOMObject(pixels);
    if (ret)
        return ret;
    
    ret = new (exec) JSCanvasPixelArray(JSCanvasPixelArrayPrototype::self(exec), pixels);
    
    exec->heap()->reportExtraMemoryCost(pixels->length());
    
    ScriptInterpreter::putDOMObject(pixels, ret);
    
    return ret;
}
    
} // namespace WebCore
