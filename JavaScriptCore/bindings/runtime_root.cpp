/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include <jni_jsobject.h>
#include <runtime_root.h>

using namespace KJS;
using namespace KJS::Bindings;

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
CFMutableDictionaryRef KJS::Bindings::findReferenceDictionary(ObjectImp *imp)
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
const Bindings::RootObject *KJS::Bindings::rootForImp (ObjectImp *imp)
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
                rootObject = (const Bindings::RootObject *)allKeys[i];
                break;
            }
        }
        
        free ((void *)allKeys);
        free ((void *)allValues);
    }
    return rootObject;
}

const Bindings::RootObject *KJS::Bindings::rootForInterpreter (KJS::Interpreter *interpreter)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary ();
    const Bindings::RootObject *aRootObject = 0, *result = 0;
    
    if (refsByRoot) {
        const void **allValues = 0;
        const void **allKeys = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(refsByRoot);
        allKeys = (const void **)malloc (sizeof(void *) * count);
        allValues = (const void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (refsByRoot, allKeys, allValues);
        for(i = 0; i < count; i++) {
            aRootObject = (const Bindings::RootObject *)allKeys[i];
            if (aRootObject->interpreter() == interpreter) {
                result = aRootObject;
                break;
            }
        }
        
        free ((void *)allKeys);
        free ((void *)allValues);
    }
    return result;
}

void KJS::Bindings::addNativeReference (const Bindings::RootObject *root, ObjectImp *imp)
{
    if (root) {
        CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (root);
        
        unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
        if (numReferences == 0) {
	    gcProtect(imp);
            CFDictionaryAddValue (referencesDictionary, imp,  (const void *)1);
        }
        else {
            CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences+1));
        }
    }
}

void KJS::Bindings::removeNativeReference (ObjectImp *imp)
{
    if (!imp)
	return;
	
    CFMutableDictionaryRef referencesDictionary = findReferenceDictionary (imp);

    if (referencesDictionary) {
        unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
        if (numReferences == 1) {
	    gcUnprotect(imp);
            CFDictionaryRemoveValue (referencesDictionary, imp);
        }
        else {
            CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences-1));
        }
    }
}

// May only be set by dispatchToJavaScriptThread().
static CFRunLoopSourceRef completionSource;

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


void RootObject::dispatchToJavaScriptThread(JSObjectCallContext *context)
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
    Bindings::RootObject::_performJavaScriptSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(Bindings::RootObject::_runLoop, Bindings::RootObject::_performJavaScriptSource, kCFRunLoopDefaultMode);
}

// Must be called when the applet is shutdown.
void RootObject::removeAllNativeReferences ()
{
    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (this);
    
    if (referencesDictionary) {
        void **allImps = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(referencesDictionary);
        allImps = (void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (referencesDictionary, (const void **)allImps, NULL);
        for(i = 0; i < count; i++) {
            ObjectImp *anImp = static_cast<ObjectImp*>(allImps[i]);
	    gcUnprotect(anImp);
        }
        free ((void *)allImps);
        CFDictionaryRemoveAllValues (referencesDictionary);

        CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary();
        CFDictionaryRemoveValue (refsByRoot, (const void *)this);
        delete this;
    }
}

void RootObject::setInterpreter (KJS::Interpreter *i)
{
    _interpreter = i;
}


