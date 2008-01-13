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

#include "identifier.h"
#include "internal.h"
#include "interpreter.h"
#include "jni_jsobject.h"
#include "jni_runtime.h"
#include "jni_utility.h"
#include "JSGlobalObject.h"
#include "list.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/Assertions.h>

using namespace KJS::Bindings;
using namespace KJS;

#ifdef NDEBUG
#define JS_LOG(formatAndArgs...) ((void)0)
#else
#define JS_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s(%p,%p):  ", __PRETTY_FUNCTION__, RootObject::runLoop(), CFRunLoopGetCurrent()); \
    fprintf(stderr, formatAndArgs); \
}
#endif

#define UndefinedHandle 1

static bool isJavaScriptThread()
{
    return (RootObject::runLoop() == CFRunLoopGetCurrent());
}

jvalue JavaJSObject::invoke (JSObjectCallContext *context)
{
    jvalue result;

    bzero ((void *)&result, sizeof(jvalue));
    
    if (!isJavaScriptThread()) {        
        // Send the call context to the thread that is allowed to
        // call JavaScript.
        RootObject::dispatchToJavaScriptThread(context);
        result = context->result;
    }
    else {
        jlong nativeHandle = context->nativeHandle;
        if (nativeHandle == UndefinedHandle || nativeHandle == 0) {
            return result;
        }

        if (context->type == CreateNative) {
            result.j = JavaJSObject::createNative(nativeHandle);
        }
        else {
            JSObject *imp = jlong_to_impptr(nativeHandle);
            if (!findProtectingRootObject(imp)) {
                fprintf (stderr, "%s:%d:  Attempt to access JavaScript from destroyed applet, type %d.\n", __FILE__, __LINE__, context->type);
                return result;
            }

            switch (context->type){            
                case Call: {
                    result.l = JavaJSObject(nativeHandle).call(context->string, context->args);
                    break;
                }
                
                case Eval: {
                    result.l = JavaJSObject(nativeHandle).eval(context->string);
                    break;
                }
            
                case GetMember: {
                    result.l = JavaJSObject(nativeHandle).getMember(context->string);
                    break;
                }
                
                case SetMember: {
                    JavaJSObject(nativeHandle).setMember(context->string, context->value);
                    break;
                }
                
                case RemoveMember: {
                    JavaJSObject(nativeHandle).removeMember(context->string);
                    break;
                }
            
                case GetSlot: {
                    result.l = JavaJSObject(nativeHandle).getSlot(context->index);
                    break;
                }
                
                case SetSlot: {
                    JavaJSObject(nativeHandle).setSlot(context->index, context->value);
                    break;
                }
            
                case ToString: {
                    result.l = (jobject) JavaJSObject(nativeHandle).toString();
                    break;
                }
    
                case Finalize: {
                    JavaJSObject(nativeHandle).finalize();
                    break;
                }
                
                default: {
                    fprintf (stderr, "%s:  invalid JavaScript call\n", __PRETTY_FUNCTION__);
                }
            }
        }
        context->result = result;
    }

    return result;
}


JavaJSObject::JavaJSObject(jlong nativeJSObject)
{
    _imp = jlong_to_impptr(nativeJSObject);
    
    ASSERT(_imp);
    _rootObject = findProtectingRootObject(_imp);
    ASSERT(_rootObject);
}

RootObject* JavaJSObject::rootObject() const
{ 
    return _rootObject && _rootObject->isValid() ? _rootObject.get() : 0; 
}

jobject JavaJSObject::call(jstring methodName, jobjectArray args) const
{
    JS_LOG ("methodName = %s\n", JavaString(methodName).UTF8String());

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;
    
    // Lookup the function object.
    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock;
    
    Identifier identifier(JavaString(methodName).ustring());
    JSValue *func = _imp->get (exec, identifier);
    if (func->isUndefinedOrNull())
        return 0;

    // Call the function object.
    JSObject *funcImp = static_cast<JSObject*>(func);
    JSObject *thisObj = const_cast<JSObject*>(_imp);
    List argList;
    getListFromJArray(args, argList);
    rootObject->globalObject()->startTimeoutCheck();
    JSValue *result = funcImp->call(exec, thisObj, argList);
    rootObject->globalObject()->stopTimeoutCheck();

    return convertValueToJObject(result);
}

jobject JavaJSObject::eval(jstring script) const
{
    JS_LOG ("script = %s\n", JavaString(script).UTF8String());
    
    JSObject *thisObj = const_cast<JSObject*>(_imp);
    JSValue *result;
    
    JSLock lock;
    
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    rootObject->globalObject()->startTimeoutCheck();
    Completion completion = Interpreter::evaluate(rootObject->globalObject()->globalExec(), UString(), 0, JavaString(script).ustring(),thisObj);
    rootObject->globalObject()->stopTimeoutCheck();
    ComplType type = completion.complType();
    
    if (type == Normal) {
        result = completion.value();
        if (!result)
            result = jsUndefined();
    } else
        result = jsUndefined();
    
    return convertValueToJObject (result);
}

jobject JavaJSObject::getMember(jstring memberName) const
{
    JS_LOG ("(%p) memberName = %s\n", _imp, JavaString(memberName).UTF8String());

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    ExecState* exec = rootObject->globalObject()->globalExec();
    
    JSLock lock;
    JSValue *result = _imp->get (exec, Identifier (JavaString(memberName).ustring()));

    return convertValueToJObject(result);
}

void JavaJSObject::setMember(jstring memberName, jobject value) const
{
    JS_LOG ("memberName = %s, value = %p\n", JavaString(memberName).UTF8String(), value);

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock;
    _imp->put(exec, Identifier (JavaString(memberName).ustring()), convertJObjectToValue(value));
}


void JavaJSObject::removeMember(jstring memberName) const
{
    JS_LOG ("memberName = %s\n", JavaString(memberName).UTF8String());

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock;
    _imp->deleteProperty(exec, Identifier (JavaString(memberName).ustring()));
}


jobject JavaJSObject::getSlot(jint index) const
{
#ifdef __LP64__
    JS_LOG ("index = %d\n", index);
#else
    JS_LOG ("index = %ld\n", index);
#endif

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    ExecState* exec = rootObject->globalObject()->globalExec();

    JSLock lock;
    JSValue *result = _imp->get (exec, (unsigned)index);

    return convertValueToJObject(result);
}


void JavaJSObject::setSlot(jint index, jobject value) const
{
#ifdef __LP64__
    JS_LOG ("index = %d, value = %p\n", index, value);
#else
    JS_LOG ("index = %ld, value = %p\n", index, value);
#endif

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock;
    _imp->put(exec, (unsigned)index, convertJObjectToValue(value));
}


jstring JavaJSObject::toString() const
{
    JS_LOG ("\n");
    
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    JSLock lock;
    JSObject *thisObj = const_cast<JSObject*>(_imp);
    ExecState* exec = rootObject->globalObject()->globalExec();
    
    return (jstring)convertValueToJValue (exec, thisObj, object_type, "java.lang.String").l;
}

void JavaJSObject::finalize() const
{
    if (RootObject* rootObject = this->rootObject())
        rootObject->gcUnprotect(_imp);
}

// We're either creating a 'Root' object (via a call to JavaJSObject.getWindow()), or
// another JavaJSObject.
jlong JavaJSObject::createNative(jlong nativeHandle)
{
    JS_LOG ("nativeHandle = %d\n", (int)nativeHandle);

    if (nativeHandle == UndefinedHandle)
        return nativeHandle;

    if (findProtectingRootObject(jlong_to_impptr(nativeHandle)))
        return nativeHandle;

    CreateRootObjectFunction createRootObject = RootObject::createRootObject();
    if (!createRootObject)
        return ptr_to_jlong(0);

    RefPtr<RootObject> rootObject = createRootObject(jlong_to_ptr(nativeHandle));

    // If rootObject is !NULL We must have been called via netscape.javascript.JavaJSObject.getWindow(),
    // otherwise we are being called after creating a JavaJSObject in
    // JavaJSObject::convertValueToJObject().
    if (rootObject) {
        JSObject* globalObject = rootObject->globalObject();
        // We call gcProtect here to get the object into the root object's "protect set" which
        // is used to test if a native handle is valid as well as getting the root object given the handle.
        rootObject->gcProtect(globalObject);
        return ptr_to_jlong(globalObject);
    }
    
    return nativeHandle;
}

jobject JavaJSObject::convertValueToJObject (JSValue *value) const
{
    JSLock lock;
    
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JNIEnv *env = getJNIEnv();
    jobject result = 0;
    
    // See section 22.7 of 'JavaScript:  The Definitive Guide, 4th Edition',
    // figure 22-5.
    // number -> java.lang.Double
    // string -> java.lang.String
    // boolean -> java.lang.Boolean
    // Java instance -> Java instance
    // Everything else -> JavaJSObject
    
    JSType type = value->type();
    if (type == NumberType) {
        jclass JSObjectClass = env->FindClass ("java/lang/Double");
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(D)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, (jdouble)value->toNumber(exec));
        }
    }
    else if (type == StringType) {
        UString stringValue = value->toString(exec);
        JNIEnv *env = getJNIEnv();
        result = env->NewString ((const jchar *)stringValue.data(), stringValue.size());
    }
    else if (type == BooleanType) {
        jclass JSObjectClass = env->FindClass ("java/lang/Boolean");
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(Z)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, (jboolean)value->toBoolean(exec));
        }
    }
    else {
        // Create a JavaJSObject.
        jlong nativeHandle;
        
        if (type == ObjectType){
            JSObject *imp = static_cast<JSObject*>(value);
            
            // We either have a wrapper around a Java instance or a JavaScript
            // object.  If we have a wrapper around a Java instance, return that
            // instance, otherwise create a new Java JavaJSObject with the JSObject*
            // as it's nativeHandle.
            if (imp->classInfo() && strcmp(imp->classInfo()->className, "RuntimeObject") == 0) {
                RuntimeObjectImp *runtimeImp = static_cast<RuntimeObjectImp*>(value);
                JavaInstance *runtimeInstance = static_cast<JavaInstance *>(runtimeImp->getInternalInstance());
                if (!runtimeInstance)
                    return 0;
                
                return runtimeInstance->javaInstance();
            }
            else {
                nativeHandle = ptr_to_jlong(imp);
                rootObject->gcProtect(imp);
            }
        }
        // All other types will result in an undefined object.
        else {
            nativeHandle = UndefinedHandle;
        }
        
        // Now create the Java JavaJSObject.  Look for the JavaJSObject in it's new (Tiger)
        // location and in the original Java 1.4.2 location.
        jclass JSObjectClass;
        
        JSObjectClass = env->FindClass ("sun/plugin/javascript/webkit/JSObject");
        if (!JSObjectClass) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            JSObjectClass = env->FindClass ("apple/applet/JSObject");
        }
            
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(J)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, nativeHandle);
        }
    }
    
    return result;
}

JSValue *JavaJSObject::convertJObjectToValue (jobject theObject) const
{
    // Instances of netscape.javascript.JSObject get converted back to
    // JavaScript objects.  All other objects are wrapped.  It's not
    // possible to pass primitive types from the Java to JavaScript.
    // See section 22.7 of 'JavaScript:  The Definitive Guide, 4th Edition',
    // figure 22-4.
    jobject classOfInstance = callJNIObjectMethod(theObject, "getClass", "()Ljava/lang/Class;");
    jstring className = (jstring)callJNIObjectMethod(classOfInstance, "getName", "()Ljava/lang/String;");
    
    // Only the sun.plugin.javascript.webkit.JSObject has a member called nativeJSObject. This class is
    // created above to wrap internal browser objects. The constructor of this class takes the native
    // pointer and stores it in this object, so that it can be retrieved below.
    if (strcmp(JavaString(className).UTF8String(), "sun.plugin.javascript.webkit.JSObject") == 0) {
        // Pull the nativeJSObject value from the Java instance.  This is a
        // pointer to the JSObject.
        JNIEnv *env = getJNIEnv();
        jfieldID fieldID = env->GetFieldID((jclass)classOfInstance, "nativeJSObject", "J");
        if (fieldID == NULL) {
            return jsUndefined();
        }
        jlong nativeHandle = env->GetLongField(theObject, fieldID);
        if (nativeHandle == UndefinedHandle) {
            return jsUndefined();
        }
        JSObject *imp = static_cast<JSObject*>(jlong_to_impptr(nativeHandle));
        return imp;
    }

    JSLock lock;
    JavaInstance* javaInstance = new JavaInstance(theObject, _rootObject);
    return KJS::Bindings::Instance::createRuntimeObject(javaInstance);
}

void JavaJSObject::getListFromJArray(jobjectArray jArray, List& list) const
{
    JNIEnv *env = getJNIEnv();
    int i, numObjects = jArray ? env->GetArrayLength (jArray) : 0;
    
    for (i = 0; i < numObjects; i++) {
        jobject anObject = env->GetObjectArrayElement ((jobjectArray)jArray, i);
        if (anObject) {
            list.append(convertJObjectToValue(anObject));
            env->DeleteLocalRef (anObject);
        }
        else {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }
}

extern "C" {

jlong KJS_JSCreateNativeJSObject (JNIEnv*, jclass, jstring, jlong nativeHandle, jboolean)
{
    JSObjectCallContext context;
    context.type = CreateNative;
    context.nativeHandle = nativeHandle;
    return JavaJSObject::invoke (&context).j;
}

void KJS_JSObject_JSFinalize (JNIEnv*, jclass, jlong nativeHandle)
{
    JSObjectCallContext context;
    context.type = Finalize;
    context.nativeHandle = nativeHandle;
    JavaJSObject::invoke (&context);
}

jobject KJS_JSObject_JSObjectCall (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring methodName, jobjectArray args, jboolean)
{
    JSObjectCallContext context;
    context.type = Call;
    context.nativeHandle = nativeHandle;
    context.string = methodName;
    context.args = args;
    return JavaJSObject::invoke (&context).l;
}

jobject KJS_JSObject_JSObjectEval (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jscript, jboolean)
{
    JSObjectCallContext context;
    context.type = Eval;
    context.nativeHandle = nativeHandle;
    context.string = jscript;
    return JavaJSObject::invoke (&context).l;
}

jobject KJS_JSObject_JSObjectGetMember (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jname, jboolean)
{
    JSObjectCallContext context;
    context.type = GetMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    return JavaJSObject::invoke (&context).l;
}

void KJS_JSObject_JSObjectSetMember (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jname, jobject value, jboolean)
{
    JSObjectCallContext context;
    context.type = SetMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    context.value = value;
    JavaJSObject::invoke (&context);
}

void KJS_JSObject_JSObjectRemoveMember (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jname, jboolean)
{
    JSObjectCallContext context;
    context.type = RemoveMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    JavaJSObject::invoke (&context);
}

jobject KJS_JSObject_JSObjectGetSlot (JNIEnv*, jclass, jlong nativeHandle, jstring, jint jindex, jboolean)
{
    JSObjectCallContext context;
    context.type = GetSlot;
    context.nativeHandle = nativeHandle;
    context.index = jindex;
    return JavaJSObject::invoke (&context).l;
}

void KJS_JSObject_JSObjectSetSlot (JNIEnv*, jclass, jlong nativeHandle, jstring, jint jindex, jobject value, jboolean)
{
    JSObjectCallContext context;
    context.type = SetSlot;
    context.nativeHandle = nativeHandle;
    context.index = jindex;
    context.value = value;
    JavaJSObject::invoke (&context);
}

jstring KJS_JSObject_JSObjectToString (JNIEnv*, jclass, jlong nativeHandle)
{
    JSObjectCallContext context;
    context.type = ToString;
    context.nativeHandle = nativeHandle;
    return (jstring)JavaJSObject::invoke (&context).l;
}

}
