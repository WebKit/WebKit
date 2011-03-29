/*
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
#include "JavaNPObjectV8.h"

#if ENABLE(JAVA_BRIDGE)

#include "JNIUtilityPrivate.h"
#include "JavaClassV8.h"
#include "JavaFieldV8.h"
#include "JavaInstanceV8.h"
#include "JavaMethod.h"
#include "JavaValueV8.h"
#include "npruntime_impl.h"

namespace JSC {

namespace Bindings {

static NPObject* AllocJavaNPObject(NPP, NPClass*)
{
    JavaNPObject* obj = static_cast<JavaNPObject*>(malloc(sizeof(JavaNPObject)));
    if (!obj)
        return 0;
    memset(obj, 0, sizeof(JavaNPObject));
    return reinterpret_cast<NPObject*>(obj);
}

static void FreeJavaNPObject(NPObject* npobj)
{
    JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(npobj);
    obj->m_instance = 0; // free does not call the destructor
    free(obj);
}

static NPClass JavaNPObjectClass = {
    NP_CLASS_STRUCT_VERSION,
    AllocJavaNPObject, // allocate,
    FreeJavaNPObject, // free,
    0, // invalidate
    JavaNPObjectHasMethod,
    JavaNPObjectInvoke,
    0, // invokeDefault,
    JavaNPObjectHasProperty,
    JavaNPObjectGetProperty,
    0, // setProperty
    0, // removeProperty
    0, // enumerate
    0 // construct
};

NPObject* JavaInstanceToNPObject(JavaInstance* instance)
{
    JavaNPObject* object = reinterpret_cast<JavaNPObject*>(_NPN_CreateObject(0, &JavaNPObjectClass));
    object->m_instance = instance;
    return reinterpret_cast<NPObject*>(object);
}

// Returns null if obj is not a wrapper of JavaInstance
JavaInstance* ExtractJavaInstance(NPObject* obj)
{
    if (obj->_class == &JavaNPObjectClass)
        return reinterpret_cast<JavaNPObject*>(obj)->m_instance.get();
    return 0;
}

bool JavaNPObjectHasMethod(NPObject* obj, NPIdentifier identifier)
{
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (!instance)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (!name)
        return false;

    instance->begin();
    bool result = (instance->getClass()->methodsNamed(name).size() > 0);
    instance->end();

    // TODO: use NPN_MemFree
    free(name);

    return result;
}

bool JavaNPObjectInvoke(NPObject* obj, NPIdentifier identifier, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (!instance)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (!name)
        return false;

    instance->begin();

    MethodList methodList = instance->getClass()->methodsNamed(name);
    // TODO: use NPN_MemFree
    free(name);

    // Try to find a good match for the overloaded method. The
    // fundamental problem is that JavaScript doesn't have the
    // notion of method overloading and Java does. We could
    // get a bit more sophisticated and attempt to do some
    // type checking as well as checking the number of parameters.
    size_t numMethods = methodList.size();
    JavaMethod* aMethod;
    JavaMethod* jMethod = 0;
    for (size_t methodIndex = 0; methodIndex < numMethods; methodIndex++) {
        aMethod = methodList[methodIndex];
        if (aMethod->numParameters() == static_cast<int>(argCount)) {
            jMethod = aMethod;
            break;
        }
    }
    if (!jMethod)
        return false;

    JavaValue* jArgs = new JavaValue[argCount];
    for (unsigned int i = 0; i < argCount; i++)
        jArgs[i] = convertNPVariantToJavaValue(args[i], jMethod->parameterAt(i));

    JavaValue jResult = instance->invokeMethod(jMethod, jArgs);
    instance->end();
    delete[] jArgs;

    VOID_TO_NPVARIANT(*result);
    convertJavaValueToNPVariant(jResult, result);
    return true;
}

bool JavaNPObjectHasProperty(NPObject* obj, NPIdentifier identifier)
{
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (!instance)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (!name)
        return false;
    instance->begin();
    bool result = instance->getClass()->fieldNamed(name);
    instance->end();
    free(name);
    return result;
}

bool JavaNPObjectGetProperty(NPObject* obj, NPIdentifier identifier, NPVariant* result)
{
    VOID_TO_NPVARIANT(*result);
    JavaInstance* instance = ExtractJavaInstance(obj);
    if (!instance)
        return false;
    NPUTF8* name = _NPN_UTF8FromIdentifier(identifier);
    if (!name)
        return false;

    instance->begin();
    JavaField* field = instance->getClass()->fieldNamed(name);
    free(name); // TODO: use NPN_MemFree
    if (!field)
        return false;

    JavaValue value = instance->getField(field);
    instance->end();

    convertJavaValueToNPVariant(value, result);

    return true;
}

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)
