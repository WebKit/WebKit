/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright 2010, The Android Open Source Project
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
#include "BridgeJSC.h"

#include "runtime_object.h"
#include "runtime_root.h"
#include <runtime/JSLock.h>

#if PLATFORM(QT)
#include "qt_instance.h"
#endif

namespace JSC {

namespace Bindings {

Array::Array(PassRefPtr<RootObject> rootObject)
    : m_rootObject(rootObject)
{
    ASSERT(m_rootObject);
}

Array::~Array()
{
}

Instance::Instance(PassRefPtr<RootObject> rootObject)
    : m_rootObject(rootObject)
    , m_runtimeObject(0)
{
    ASSERT(m_rootObject);
}

Instance::~Instance()
{
    ASSERT(!m_runtimeObject);
}

static KJSDidExecuteFunctionPtr s_didExecuteFunction;

void Instance::setDidExecuteFunction(KJSDidExecuteFunctionPtr func)
{
    s_didExecuteFunction = func;
}

KJSDidExecuteFunctionPtr Instance::didExecuteFunction()
{
    return s_didExecuteFunction;
}

void Instance::begin()
{
    virtualBegin();
}

void Instance::end()
{
    virtualEnd();
}

RuntimeObject* Instance::createRuntimeObject(ExecState* exec)
{
    ASSERT(m_rootObject);
    ASSERT(m_rootObject->isValid());
    if (m_runtimeObject)
        return m_runtimeObject;
    JSLock lock(SilenceAssertionsOnly);
    m_runtimeObject = newRuntimeObject(exec);
    m_rootObject->addRuntimeObject(m_runtimeObject);
    return m_runtimeObject;
}

RuntimeObject* Instance::newRuntimeObject(ExecState* exec)
{
    JSLock lock(SilenceAssertionsOnly);
    return new (exec)RuntimeObject(exec, this);
}

void Instance::willDestroyRuntimeObject()
{
    ASSERT(m_rootObject);
    ASSERT(m_rootObject->isValid());
    ASSERT(m_runtimeObject);
    m_rootObject->removeRuntimeObject(m_runtimeObject);
    m_runtimeObject = 0;
}

void Instance::willInvalidateRuntimeObject()
{
    ASSERT(m_runtimeObject);
    m_runtimeObject = 0;
}

RootObject* Instance::rootObject() const
{
    return m_rootObject && m_rootObject->isValid() ? m_rootObject.get() : 0;
}

} // namespace Bindings

} // namespace JSC
