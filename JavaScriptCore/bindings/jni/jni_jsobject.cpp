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
#include <CoreFoundation/CoreFoundation.h>

#include <assert.h>

#include <identifier.h>
#include <internal.h>
#include <interpreter.h>
#include <list.h>
#include <jni_jsobject.h>
#include <jni_runtime.h>
#include <jni_utility.h>
#include <runtime_object.h>
#include <runtime_root.h>

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

jvalue JSObject::invoke (JSObjectCallContext *context)
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
            result.j = JSObject::createNative(nativeHandle);
        }
        else {
            KJS::ObjectImp *imp = jlong_to_impptr(nativeHandle);
            if (!rootForImp(imp)) {
                fprintf (stderr, "%s:%d:  Attempt to access JavaScript from destroyed applet, type %d.\n", __FILE__, __LINE__, context->type);
                return result;
            }

            switch (context->type){            
                case Call: {
                    result.l = JSObject(nativeHandle).call(context->string, context->args);
                    break;
                }
                
                case Eval: {
                    result.l = JSObject(nativeHandle).eval(context->string);
                    break;
                }
            
                case GetMember: {
                    result.l = JSObject(nativeHandle).getMember(context->string);
                    break;
                }
                
                case SetMember: {
                    JSObject(nativeHandle).setMember(context->string, context->value);
                    break;
                }
                
                case RemoveMember: {
                    JSObject(nativeHandle).removeMember(context->string);
                    break;
                }
            
                case GetSlot: {
                    result.l = JSObject(nativeHandle).getSlot(context->index);
                    break;
                }
                
                case SetSlot: {
                    JSObject(nativeHandle).setSlot(context->index, context->value);
                    break;
                }
            
                case ToString: {
                    result.l = (jobject) JSObject(nativeHandle).toString();
                    break;
                }
    
                case Finalize: {
                    ObjectImp *imp = jlong_to_impptr(nativeHandle);
                    if (findReferenceDictionary(imp) == 0) {
                        // We may have received a finalize method call from the VM 
                        // AFTER removing our last reference to the Java instance.
                        JS_LOG ("finalize called on instance we have already removed.\n");
                    }
                    else {
                        JSObject(nativeHandle).finalize();
                    }
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


JSObject::JSObject(jlong nativeJSObject)
{
    _imp = jlong_to_impptr(nativeJSObject);
    
    // If we are unable to cast the nativeJSObject to an ObjectImp something is
    // terribly wrong.
    assert (_imp != 0);
    
    _root = rootForImp(_imp);
    
    // If we can't find the root for the object something is terribly wrong.
    assert (_root != 0);
}


jobject JSObject::call(jstring methodName, jobjectArray args) const
{
    JS_LOG ("methodName = %s\n", JavaString(methodName).UTF8String());

    // Lookup the function object.
    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    
    Identifier identifier(JavaString(methodName).ustring());
    Value func = _imp->get (exec, identifier);
    Interpreter::unlock();
    if (func.isNull() || func.type() == UndefinedType) {
        // Maybe throw an exception here?
        return 0;
    }

    // Call the function object.
    ObjectImp *funcImp = static_cast<ObjectImp*>(func.imp());
    Object thisObj = Object(const_cast<ObjectImp*>(_imp));
    List argList = listFromJArray(args);
    Interpreter::lock();
    Value result = funcImp->call (exec, thisObj, argList);
    Interpreter::unlock();

    // Convert and return the result of the function call.
    return convertValueToJObject (result);
}

jobject JSObject::eval(jstring script) const
{
    JS_LOG ("script = %s\n", JavaString(script).UTF8String());

    Object thisObj = Object(const_cast<ObjectImp*>(_imp));
    Value result;
    
    Interpreter::lock();

    Completion completion = _root->interpreter()->evaluate(UString(), 0, JavaString(script).ustring(),thisObj);
    ComplType type = completion.complType();
    
    if (type == Normal) {
        result = completion.value();
        if (result.isNull()) {
            result = Undefined();
        }
    }
    else
        result = Undefined();

    Interpreter::unlock();
    
    return convertValueToJObject (result);
}

jobject JSObject::getMember(jstring memberName) const
{
    JS_LOG ("(%p) memberName = %s\n", _imp, JavaString(memberName).UTF8String());

    ExecState *exec = _root->interpreter()->globalExec();

    Interpreter::lock();
    Value result = _imp->get (exec, Identifier (JavaString(memberName).ustring()));
    Interpreter::unlock();

    return convertValueToJObject (result);
}

void JSObject::setMember(jstring memberName, jobject value) const
{
    JS_LOG ("memberName = %s, value = %p\n", JavaString(memberName).UTF8String(), value);
    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    _imp->put (exec, Identifier (JavaString(memberName).ustring()), convertJObjectToValue(value));
    Interpreter::unlock();
}


void JSObject::removeMember(jstring memberName) const
{
    JS_LOG ("memberName = %s\n", JavaString(memberName).UTF8String());

    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    _imp->deleteProperty (exec, Identifier (JavaString(memberName).ustring()));
    Interpreter::unlock();
}


jobject JSObject::getSlot(jint index) const
{
    JS_LOG ("index = %ld\n", index);

    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = _imp->get (exec, (unsigned)index);
    Interpreter::unlock();

    return convertValueToJObject (result);
}


void JSObject::setSlot(jint index, jobject value) const
{
    JS_LOG ("index = %ld, value = %p\n", index, value);

    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    _imp->put (exec, (unsigned)index, convertJObjectToValue(value));
    Interpreter::unlock();
}


jstring JSObject::toString() const
{
    JS_LOG ("\n");

    Interpreter::lock();
    Object thisObj = Object(const_cast<ObjectImp*>(_imp));
    ExecState *exec = _root->interpreter()->globalExec();
    
    jstring result = (jstring)convertValueToJValue (exec, thisObj, object_type, "java.lang.String").l;

    Interpreter::unlock();
    
    return result;
}

void JSObject::finalize() const
{
    JS_LOG ("\n");

    removeNativeReference (_imp);
}

// We're either creating a 'Root' object (via a call to JSObject.getWindow()), or
// another JSObject.
jlong JSObject::createNative(jlong nativeHandle)
{
    JS_LOG ("nativeHandle = %d\n", (int)nativeHandle);

    if (nativeHandle == UndefinedHandle)
        return nativeHandle;
    else if (rootForImp(jlong_to_impptr(nativeHandle))){
        return nativeHandle;
    }

    FindRootObjectForNativeHandleFunctionPtr aFunc = RootObject::findRootObjectForNativeHandleFunction();
    if (aFunc) {
        Bindings::RootObject *root = aFunc(jlong_to_ptr(nativeHandle));
        // If root is !NULL We must have been called via netscape.javascript.JSObject.getWindow(),
        // otherwise we are being called after creating a JSObject in
        // JSObject::convertValueToJObject().
        if (root) {
            addNativeReference (root, root->rootObjectImp());        
            return ptr_to_jlong(root->rootObjectImp());
        }
        else {
            return nativeHandle;
        }
    }
    
    return ptr_to_jlong(0);
}

jobject JSObject::convertValueToJObject (KJS::Value value) const
{
    ExecState *exec = _root->interpreter()->globalExec();
    JNIEnv *env = getJNIEnv();
    jobject result = 0;
    
    // See section 22.7 of 'JavaScript:  The Definitive Guide, 4th Edition',
    // figure 22-5.
    // number -> java.lang.Double
    // string -> java.lang.String
    // boolean -> java.lang.Boolean
    // Java instance -> Java instance
    // Everything else -> JSObject
    
    KJS::Type type = value.type();
    if (type == KJS::NumberType) {
        jclass JSObjectClass = env->FindClass ("java/lang/Double");
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(D)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, (jdouble)value.toNumber(exec));
        }
    }
    else if (type == KJS::StringType) {
        KJS::UString stringValue = value.toString(exec);
        JNIEnv *env = getJNIEnv();
        result = env->NewString ((const jchar *)stringValue.data(), stringValue.size());
    }
    else if (type == KJS::BooleanType) {
        jclass JSObjectClass = env->FindClass ("java/lang/Boolean");
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(Z)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, (jboolean)value.toBoolean(exec));
        }
    }
    else {
        // Create a JSObject.
        jlong nativeHandle;
        
        if (type == KJS::ObjectType){
            KJS::ObjectImp *imp = static_cast<KJS::ObjectImp*>(value.imp());
            
            // We either have a wrapper around a Java instance or a JavaScript
            // object.  If we have a wrapper around a Java instance, return that
            // instance, otherwise create a new Java JSObject with the ObjectImp*
            // as it's nativeHandle.
            if (imp->classInfo() && strcmp(imp->classInfo()->className, "RuntimeObject") == 0) {
                KJS::RuntimeObjectImp *runtimeImp = static_cast<KJS::RuntimeObjectImp*>(value.imp());
                Bindings::JavaInstance *runtimeInstance = static_cast<Bindings::JavaInstance *>(runtimeImp->getInternalInstance());
                return runtimeInstance->javaInstance();
            }
            else {
                nativeHandle = ptr_to_jlong(imp);
                
                // Bump our 'meta' reference count for the imp.  We maintain the reference
                // until either finalize is called or the applet shuts down.
                addNativeReference (_root, imp);
            }
        }
        // All other types will result in an undefined object.
        else {
            nativeHandle = UndefinedHandle;
        }
        
        // Now create the Java JSObject.  Look for the JSObject in it's new (Tiger)
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

KJS::Value JSObject::convertJObjectToValue (jobject theObject) const
{
    // Instances of netscape.javascript.JSObject get converted back to
    // JavaScript objects.  All other objects are wrapped.  It's not
    // possible to pass primitive types from the Java to JavaScript.
    // See section 22.7 of 'JavaScript:  The Definitive Guide, 4th Edition',
    // figure 22-4.
    jobject classOfInstance = callJNIObjectMethod(theObject, "getClass", "()Ljava/lang/Class;");
    jstring className = (jstring)callJNIObjectMethod(classOfInstance, "getName", "()Ljava/lang/String;");
        
    if (strcmp(Bindings::JavaString(className).UTF8String(), "netscape.javascript.JSObject") == 0) {
        // Pull the nativeJSObject value from the Java instance.  This is a
        // pointer to the ObjectImp.
        JNIEnv *env = getJNIEnv();
        jfieldID fieldID = env->GetFieldID((jclass)classOfInstance, "nativeJSObject", "long");
        if (fieldID == NULL) {
            return KJS::Undefined();
        }
        jlong nativeHandle = env->GetLongField(theObject, fieldID);
        if (nativeHandle == UndefinedHandle) {
            return KJS::Undefined();
        }
        KJS::ObjectImp *imp = static_cast<KJS::ObjectImp*>(jlong_to_impptr(nativeHandle));
        return KJS::Object(const_cast<KJS::ObjectImp*>(imp));
    }

    Interpreter::lock();
    KJS::RuntimeObjectImp *newImp = new KJS::RuntimeObjectImp(new Bindings::JavaInstance (theObject, _root));
    Interpreter::unlock();

    return KJS::Object(newImp);
}

KJS::List JSObject::listFromJArray(jobjectArray jArray) const
{
    JNIEnv *env = getJNIEnv();
    long i, numObjects = jArray ? env->GetArrayLength (jArray) : 0;
    KJS::List aList;
    
    for (i = 0; i < numObjects; i++) {
        jobject anObject = env->GetObjectArrayElement ((jobjectArray)jArray, i);
	if (anObject) {
	    aList.append (convertJObjectToValue(anObject));
	    env->DeleteLocalRef (anObject);
	}
	else {
            env->ExceptionDescribe();
            env->ExceptionClear();
	}
    }
    return aList;
}

extern "C" {

jlong KJS_JSCreateNativeJSObject (JNIEnv *env, jclass clazz, jstring jurl, jlong nativeHandle, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = CreateNative;
    context.nativeHandle = nativeHandle;
    return JSObject::invoke (&context).j;
}

void KJS_JSObject_JSFinalize (JNIEnv *env, jclass jsClass, jlong nativeHandle)
{
    JSObjectCallContext context;
    context.type = Finalize;
    context.nativeHandle = nativeHandle;
    JSObject::invoke (&context);
}

jobject KJS_JSObject_JSObjectCall (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jstring methodName, jobjectArray args, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = Call;
    context.nativeHandle = nativeHandle;
    context.string = methodName;
    context.args = args;
    return JSObject::invoke (&context).l;
}

jobject KJS_JSObject_JSObjectEval (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jstring jscript, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = Eval;
    context.nativeHandle = nativeHandle;
    context.string = jscript;
    return JSObject::invoke (&context).l;
}

jobject KJS_JSObject_JSObjectGetMember (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jstring jname, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = GetMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    return JSObject::invoke (&context).l;
}

void KJS_JSObject_JSObjectSetMember (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jstring jname, jobject value, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = SetMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    context.value = value;
    JSObject::invoke (&context);
}

void KJS_JSObject_JSObjectRemoveMember (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jstring jname, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = RemoveMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    JSObject::invoke (&context);
}

jobject KJS_JSObject_JSObjectGetSlot (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jint jindex, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = GetSlot;
    context.nativeHandle = nativeHandle;
    context.index = jindex;
    return JSObject::invoke (&context).l;
}

void KJS_JSObject_JSObjectSetSlot (JNIEnv *env, jclass jsClass, jlong nativeHandle, jstring jurl, jint jindex, jobject value, jboolean ctx)
{
    JSObjectCallContext context;
    context.type = SetSlot;
    context.nativeHandle = nativeHandle;
    context.index = jindex;
    context.value = value;
    JSObject::invoke (&context);
}

jstring KJS_JSObject_JSObjectToString (JNIEnv *env, jclass clazz, jlong nativeHandle)
{
    JSObjectCallContext context;
    context.type = ToString;
    context.nativeHandle = nativeHandle;
    return (jstring)JSObject::invoke (&context).l;
}

}
