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

// Java does NOT always call finalize (and thus KJS_JSObject_JSFinalize) when
// it collects an objects.  This presents some difficulties.  We must ensure
// the a JSObject's corresponding JavaScript object doesn't get collected.  We
// do this by incrementing the JavaScript's reference count the first time we
// create a JSObject for it, and decrementing the JavaScript reference count when
// the last JSObject that refers to it is finalized, or when the applet is
// shutdown.
//
// To do this we keep a dictionary that maps each applet instance
// to the JavaScript objects it is referencing.  For each JavaScript instance
// we also maintain a secondary reference count.  When that reference count reaches
// 1 OR the applet is shutdown we deref the JavaScript instance.  Applet instances
// are represented by a jlong.

static CFMutableDictionaryRef referencesByRootDictionary = 0;

static CFMutableDictionaryRef getReferencesByRootDictionary()
{
    if (!referencesByRootDictionary)
        referencesByRootDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    return referencesByRootDictionary;
}

static CFMutableDictionaryRef getReferencesDictionary(const Bindings::RootObject *root)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary();
    CFMutableDictionaryRef referencesDictionary = 0;
    
    referencesDictionary = (CFMutableDictionaryRef)CFDictionaryGetValue (refsByRoot, (const void *)root);
    if (!referencesDictionary) {
        referencesDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
        CFDictionaryAddValue (refsByRoot, root, referencesDictionary);
        CFRelease (referencesDictionary);
    }
    return referencesDictionary;
}

// Scan all the dictionary for all the roots to see if any have a 
// reference to the imp, and if so, return it's reference count
// dictionary.
// FIXME:  This is a potential performance bottleneck with many applets.  We could fix be adding a
// imp to root dictionary.
static CFMutableDictionaryRef findReferenceDictionary(ObjectImp *imp)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary ();
    CFMutableDictionaryRef foundDictionary = 0;
    
    if (refsByRoot) {
        const void **allValues = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(refsByRoot);
        allValues = (const void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (refsByRoot, NULL, allValues);
        for(i = 0; i < count; i++) {
            CFMutableDictionaryRef referencesDictionary = (CFMutableDictionaryRef)allValues[i];
            if (CFDictionaryGetValue(referencesDictionary, imp) != 0) {
                foundDictionary = referencesDictionary;
                break;
            }
        }
        
        free ((void *)allValues);
    }
    return foundDictionary;
}

// FIXME:  This is a potential performance bottleneck with many applets.  We could fix be adding a
// imp to root dictionary.
static const Bindings::RootObject *rootForImp (ObjectImp *imp)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary ();
    const Bindings::RootObject *rootObject = 0;
    
    if (refsByRoot) {
        const void **allValues = 0;
        const void **allKeys = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(refsByRoot);
        allKeys = (const void **)malloc (sizeof(void *) * count);
        allValues = (const void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (refsByRoot, allKeys, allValues);
        for(i = 0; i < count; i++) {
            CFMutableDictionaryRef referencesDictionary = (CFMutableDictionaryRef)allValues[i];
            if (CFDictionaryGetValue(referencesDictionary, imp) != 0) {
                rootObject = (const Bindings::RootObject *)allKeys[0];
                break;
            }
        }
        
        free ((void *)allKeys);
        free ((void *)allValues);
    }
    return rootObject;
}

static void addJavaReference (const Bindings::RootObject *root, ObjectImp *imp)
{
    JS_LOG ("root = %p, imp %p\n", root, imp);

    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (root);
    
    unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
    if (numReferences == 0) {
        imp->ref();
        CFDictionaryAddValue (referencesDictionary, imp,  (const void *)1);
    }
    else {
        CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences+1));
    }
}

static void removeJavaReference (ObjectImp *imp)
{
    JS_LOG ("imp %p\n", imp);
    CFMutableDictionaryRef referencesDictionary = findReferenceDictionary (imp);
    
    unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
    if (numReferences == 1) {
        imp->deref();
        CFDictionaryRemoveValue (referencesDictionary, imp);
    }
    else {
        CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences-1));
    }
}

// May only be set by dispatchToJavaScriptThread().
static CFRunLoopSourceRef completionSource;

static void performJavaScriptAccess(void *info);
static void performJavaScriptAccess(void *i)
{
    assert (CFRunLoopGetCurrent() == RootObject::runLoop());
    
    // Dispatch JavaScript calls here.
    CFRunLoopSourceContext sourceContext;
    CFRunLoopSourceGetContext (completionSource, &sourceContext);
    JSObjectCallContext *callContext = (JSObjectCallContext *)sourceContext.info;    
    CFRunLoopRef originatingLoop = callContext->originatingLoop;

    JSObject::invoke (callContext);
    
    // Signal the originating thread that we're done.
    CFRunLoopSourceSignal (completionSource);
    if (CFRunLoopIsWaiting(originatingLoop)) {
        CFRunLoopWakeUp(originatingLoop);
    }
}

static void completedJavaScriptAccess (void *i);
static void completedJavaScriptAccess (void *i)
{
    assert (CFRunLoopGetCurrent() != RootObject::runLoop());

    JSObjectCallContext *callContext = (JSObjectCallContext *)i;
    CFRunLoopRef runLoop = (CFRunLoopRef)callContext->originatingLoop;

    assert (CFRunLoopGetCurrent() == runLoop);

    CFRunLoopStop(runLoop);
}

static pthread_once_t javaScriptAccessLockOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t javaScriptAccessLock;
static int javaScriptAccessLockCount = 0;

static void initializeJavaScriptAccessLock()
{
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    
    pthread_mutex_init(&javaScriptAccessLock, &attr);
}

static inline void lockJavaScriptAccess()
{
    // Perhaps add deadlock detection?
    pthread_once(&javaScriptAccessLockOnce, initializeJavaScriptAccessLock);
    pthread_mutex_lock(&javaScriptAccessLock);
    javaScriptAccessLockCount++;
}

static inline void unlockJavaScriptAccess()
{
    javaScriptAccessLockCount--;
    pthread_mutex_unlock(&javaScriptAccessLock);
}


static void dispatchToJavaScriptThread(JSObjectCallContext *context)
{
    // This lock guarantees that only one thread can invoke
    // at a time, and also guarantees that completionSource;
    // won't get clobbered.
    lockJavaScriptAccess();

    CFRunLoopRef currentRunLoop = CFRunLoopGetCurrent();

    assert (currentRunLoop != RootObject::runLoop());

    // Setup a source to signal once the invocation of the JavaScript
    // call completes.
    //
    // FIXME:  This could be a potential performance issue.  Creating and
    // adding run loop sources is expensive.  We could create one source 
    // per thread, as needed, instead.
    context->originatingLoop = currentRunLoop;
    CFRunLoopSourceContext sourceContext = {0, context, NULL, NULL, NULL, NULL, NULL, NULL, NULL, completedJavaScriptAccess};
    completionSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(currentRunLoop, completionSource, kCFRunLoopDefaultMode);

    // Wakeup JavaScript access thread and make it do it's work.
    CFRunLoopSourceSignal(RootObject::performJavaScriptSource());
    if (CFRunLoopIsWaiting(RootObject::runLoop())) {
        CFRunLoopWakeUp(RootObject::runLoop());
    }
    
    // Wait until the JavaScript access thread is done.
    CFRunLoopRun ();

    CFRunLoopRemoveSource(currentRunLoop, completionSource, kCFRunLoopDefaultMode);
    CFRelease (completionSource);

    unlockJavaScriptAccess();
}

FindRootObjectForNativeHandleFunctionPtr RootObject::_findRootObjectForNativeHandleFunctionPtr = 0;
CFRunLoopRef RootObject::_runLoop = 0;
CFRunLoopSourceRef RootObject::_performJavaScriptSource = 0;

// Must be called from the thread that will be used to access JavaScript.
void RootObject::setFindRootObjectForNativeHandleFunction(FindRootObjectForNativeHandleFunctionPtr aFunc) {
    // Should only be called once.
    assert (_findRootObjectForNativeHandleFunctionPtr == 0);

    _findRootObjectForNativeHandleFunctionPtr = aFunc;
    
    // Assume that we can retain this run loop forever.  It'll most 
    // likely (always?) be the main loop.
    _runLoop = (CFRunLoopRef)CFRetain (CFRunLoopGetCurrent ());

    // Setup a source the other threads can use to signal the _runLoop
    // thread that a JavaScript call needs to be invoked.
    CFRunLoopSourceContext sourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, performJavaScriptAccess};
    _performJavaScriptSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(_runLoop, _performJavaScriptSource, kCFRunLoopDefaultMode);
}

// Must be called when the applet is shutdown.
void RootObject::removeAllJavaReferencesForRoot (Bindings::RootObject *root)
{
    JS_LOG ("_root == %p\n", root);
    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (root);
    
    if (referencesDictionary) {
        void **allImps = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(referencesDictionary);
        allImps = (void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (referencesDictionary, (const void **)allImps, NULL);
        for(i = 0; i < count; i++) {
            ObjectImp *anImp = static_cast<ObjectImp*>(allImps[i]);
            anImp->deref();
        }
        free ((void *)allImps);
        CFDictionaryRemoveAllValues (referencesDictionary);

        CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary();
        CFDictionaryRemoveValue (refsByRoot, (const void *)root);
        delete root;
    }
}

jvalue JSObject::invoke (JSObjectCallContext *context)
{
    jvalue result;

    if (!isJavaScriptThread()) {        
        // Send the call context to the thread that is allowed to
        // call JavaScript.
        dispatchToJavaScriptThread(context);
        result = context->result;
    }
    else {
        jlong nativeHandle = context->nativeHandle;
        if (nativeHandle == UndefinedHandle || nativeHandle == 0) {
            bzero ((void *)&result, sizeof(jvalue));
            return result;
        }

        switch (context->type){
            case CreateNative: {
                result.j = JSObject::createNative(nativeHandle);
                break;
            }
        
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
    
    // If we can't find the root for the object something is terrible wrong.
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
    Interpreter::lock();
    KJS::Value result = _root->interpreter()->evaluate(JavaString(script).ustring(),thisObj).value();
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
    JS_LOG ("index = %d\n", index);

    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = _imp->get (exec, (unsigned)index);
    Interpreter::unlock();

    return convertValueToJObject (result);
}


void JSObject::setSlot(jint index, jobject value) const
{
    JS_LOG ("index = %d, value = %p\n", index, value);

    ExecState *exec = _root->interpreter()->globalExec();
    Interpreter::lock();
    _imp->put (exec, (unsigned)index, convertJObjectToValue(value));
    Interpreter::unlock();
}


jstring JSObject::toString() const
{
    JS_LOG ("\n");

    Object thisObj = Object(const_cast<ObjectImp*>(_imp));
    ExecState *exec = _root->interpreter()->globalExec();
    
    return (jstring)convertValueToJValue (exec, thisObj, object_type, "java.lang.String").l;
}

void JSObject::finalize() const
{
    JS_LOG ("\n");

    removeJavaReference (_imp);
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
            addJavaReference (root, root->rootObjectImp());        
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
                addJavaReference (_root, imp);
            }
        }
        // All other types will result in an undefined object.
        else {
            nativeHandle = UndefinedHandle;
        }
        
        // Now create the Java JSObject.
        jclass JSObjectClass = env->FindClass ("apple/applet/JSObject");
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
    
    JS_LOG ("converting instance of class %s\n", Bindings::JavaString(className).UTF8String());
    
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
    KJS::RuntimeObjectImp *newImp = new KJS::RuntimeObjectImp(new Bindings::JavaInstance (theObject));
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
        aList.append (convertJObjectToValue(anObject));
        env->DeleteLocalRef (anObject);
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
