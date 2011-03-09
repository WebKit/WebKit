/*
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JavaInstanceV8.h"

#if ENABLE(JAVA_BRIDGE)

#include "JavaMethod.h"
#include "JNIUtilityPrivate.h"
#include "JavaClassV8.h"

#include <wtf/text/CString.h>

using namespace JSC::Bindings;

JavaInstance::JavaInstance(jobject instance)
{
    m_instance = new JobjectWrapper(instance);
    m_class = 0;
}

JavaInstance::~JavaInstance()
{
    m_instance = 0;
    delete m_class;
}

#define NUM_LOCAL_REFS 64

void JavaInstance::virtualBegin()
{
    getJNIEnv()->PushLocalFrame(NUM_LOCAL_REFS);
}

void JavaInstance::virtualEnd()
{
    getJNIEnv()->PopLocalFrame(0);
}

JavaClass* JavaInstance::getClass() const
{
    if (!m_class)
        m_class = new JavaClass(javaInstance());
    return m_class;
}

jvalue JavaInstance::invokeMethod(const JavaMethod* method, jvalue* args)
{
    ASSERT(getClass()->methodsNamed(method->name().utf8()).find(method) != notFound);
    return callJNIMethod(javaInstance(), method->JNIReturnType(), method->name().utf8(), method->signature(), args);
}

#endif // ENABLE(JAVA_BRIDGE)
