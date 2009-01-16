/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include "jni_class.h"

#if ENABLE(MAC_JAVA_BRIDGE)

#include "JSDOMWindow.h"
#include <runtime/Identifier.h>
#include <runtime/JSLock.h>
#include "jni_utility.h"
#include "jni_runtime.h"

using namespace JSC::Bindings;

JavaClass::JavaClass(jobject anInstance)
{
    jobject aClass = callJNIMethod<jobject>(anInstance, "getClass", "()Ljava/lang/Class;");
    
    if (!aClass) {
        fprintf(stderr, "%s:  unable to call getClass on instance %p\n", __PRETTY_FUNCTION__, anInstance);
        return;
    }
    
    jstring className = (jstring)callJNIMethod<jobject>(aClass, "getName", "()Ljava/lang/String;");
    const char *classNameC = getCharactersFromJString(className);
    _name = strdup(classNameC);
    releaseCharactersForJString(className, classNameC);

    int i;
    JNIEnv *env = getJNIEnv();

    // Get the fields
    jarray fields = (jarray)callJNIMethod<jobject>(aClass, "getFields", "()[Ljava/lang/reflect/Field;");
    int numFields = env->GetArrayLength(fields);    
    for (i = 0; i < numFields; i++) {
        jobject aJField = env->GetObjectArrayElement((jobjectArray)fields, i);
        JavaField *aField = new JavaField(env, aJField); // deleted in the JavaClass destructor
        {
            JSLock lock(false);
            _fields.set(aField->name(), aField);
        }
        env->DeleteLocalRef(aJField);
    }

    // Get the methods
    jarray methods = (jarray)callJNIMethod<jobject>(aClass, "getMethods", "()[Ljava/lang/reflect/Method;");
    int numMethods = env->GetArrayLength(methods);
    for (i = 0; i < numMethods; i++) {
        jobject aJMethod = env->GetObjectArrayElement((jobjectArray)methods, i);
        JavaMethod *aMethod = new JavaMethod(env, aJMethod); // deleted in the JavaClass destructor
        MethodList* methodList;
        {
            JSLock lock(false);

            methodList = _methods.get(aMethod->name());
            if (!methodList) {
                methodList = new MethodList();
                _methods.set(aMethod->name(), methodList);
            }
        }
        methodList->append(aMethod);
        env->DeleteLocalRef(aJMethod);
    }    
}

JavaClass::~JavaClass() {
    free((void *)_name);

    JSLock lock(false);

    deleteAllValues(_fields);
    _fields.clear();

    MethodListMap::const_iterator end = _methods.end();
    for (MethodListMap::const_iterator it = _methods.begin(); it != end; ++it) {
        const MethodList* methodList = it->second;
        deleteAllValues(*methodList);
        delete methodList;
    }
    _methods.clear();
}

MethodList JavaClass::methodsNamed(const Identifier& identifier, Instance*) const
{
    MethodList *methodList = _methods.get(identifier.ustring().rep());
    
    if (methodList)
        return *methodList;
    return MethodList();
}

Field *JavaClass::fieldNamed(const Identifier& identifier, Instance*) const
{
    return _fields.get(identifier.ustring().rep());
}

bool JavaClass::isNumberClass() const
{
    return ((strcmp(_name, "java.lang.Byte") == 0 ||
             strcmp(_name, "java.lang.Short") == 0 ||
             strcmp(_name, "java.lang.Integer") == 0 ||
             strcmp(_name, "java.lang.Long") == 0 ||
             strcmp(_name, "java.lang.Float") == 0 ||
             strcmp(_name, "java.lang.Double") == 0) );
}

bool JavaClass::isBooleanClass() const
{
    return strcmp(_name, "java.lang.Boolean") == 0;
}

bool JavaClass::isStringClass() const
{
    return strcmp(_name, "java.lang.String") == 0;
}

#endif // ENABLE(MAC_JAVA_BRIDGE)
