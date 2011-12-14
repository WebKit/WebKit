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
#include "JavaInstanceJobjectV8.h"

#if ENABLE(JAVA_BRIDGE)

#include "JNIUtilityPrivate.h"
#include "JavaClassJobjectV8.h"
#include "JavaFieldV8.h"
#include "JavaMethod.h"

#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

using namespace JSC::Bindings;

JavaInstanceJobject::JavaInstanceJobject(jobject instance)
    : m_instance(new JobjectWrapper(instance))
{
}

#define NUM_LOCAL_REFS 64

void JavaInstanceJobject::begin()
{
    getJNIEnv()->PushLocalFrame(NUM_LOCAL_REFS);
}

void JavaInstanceJobject::end()
{
    getJNIEnv()->PopLocalFrame(0);
}

JavaClass* JavaInstanceJobject::getClass() const
{
    if (!m_class)
        m_class = adoptPtr(new JavaClassJobject(javaInstance()));
    return m_class.get();
}

JavaValue JavaInstanceJobject::invokeMethod(const JavaMethod* method, JavaValue* args)
{
    ASSERT(getClass()->methodsNamed(method->name().utf8().data()).find(method) != notFound);
    unsigned int numParams = method->numParameters();
    OwnArrayPtr<jvalue> jvalueArgs = adoptArrayPtr(new jvalue[numParams]);
    for (unsigned int i = 0; i < numParams; ++i)
        jvalueArgs[i] = javaValueToJvalue(args[i]);
    jvalue result = callJNIMethod(javaInstance(), method->returnType(), method->name().utf8().data(), method->signature(), jvalueArgs.get());
    return jvalueToJavaValue(result, method->returnType());
}

JavaValue JavaInstanceJobject::getField(const JavaField* field)
{
    ASSERT(getClass()->fieldNamed(field->name().utf8().data()) == field);
    return jvalueToJavaValue(getJNIField(javaInstance(), field->type(), field->name().utf8().data(), field->typeClassName()), field->type());
}

#endif // ENABLE(JAVA_BRIDGE)
