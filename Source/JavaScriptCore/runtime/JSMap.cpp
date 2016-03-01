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
#include "JSMap.h"

#include "CopiedBlockInlines.h"
#include "JSCJSValueInlines.h"
#include "JSMapIterator.h"
#include "MapDataInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSMap::s_info = { "Map", &Base::s_info, 0, CREATE_METHOD_TABLE(JSMap) };

void JSMap::destroy(JSCell* cell)
{
    JSMap* thisObject = jsCast<JSMap*>(cell);
    thisObject->JSMap::~JSMap();
}

size_t JSMap::estimatedSize(JSCell* cell)
{
    JSMap* thisObject = jsCast<JSMap*>(cell);
    size_t mapDataSize = thisObject->m_mapData.capacityInBytes();
    return Base::estimatedSize(cell) + mapDataSize;
}

void JSMap::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Base::visitChildren(cell, visitor);
    jsCast<JSMap*>(cell)->m_mapData.visitChildren(cell, visitor);
}

void JSMap::copyBackingStore(JSCell* cell, CopyVisitor& visitor, CopyToken token)
{
    Base::copyBackingStore(cell, visitor, token);
    jsCast<JSMap*>(cell)->m_mapData.copyBackingStore(visitor, token);
}

bool JSMap::has(ExecState* exec, JSValue key)
{
    return m_mapData.contains(exec, key);
}

size_t JSMap::size(ExecState* exec)
{
    return m_mapData.size(exec);
}

JSValue JSMap::get(ExecState* exec, JSValue key)
{
    JSValue result = m_mapData.get(exec, key);
    if (!result)
        return jsUndefined();
    return result;
}

void JSMap::set(ExecState* exec, JSValue key, JSValue value)
{
    m_mapData.set(exec, this, key, value);
}

void JSMap::clear(ExecState*)
{
    m_mapData.clear();
}

bool JSMap::remove(ExecState* exec, JSValue key)
{
    return m_mapData.remove(exec, key);
}

}
