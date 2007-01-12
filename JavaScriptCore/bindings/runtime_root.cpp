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
#include "config.h"

#include "object.h"
#include "runtime_root.h"
#include <wtf/HashCountedSet.h>

namespace KJS { namespace Bindings {

// Java does NOT always call finalize (and thus KJS_JSObject_JSFinalize) when
// it collects an objects.  This presents some difficulties.  We must ensure
// the a JavaJSObject's corresponding JavaScript object doesn't get collected.  We
// do this by incrementing the JavaScript's reference count the first time we
// create a JavaJSObject for it, and decrementing the JavaScript reference count when
// the last JavaJSObject that refers to it is finalized, or when the applet is
// shutdown.
//
// To do this we keep a map that maps each applet instance
// to the JavaScript objects it is referencing.  For each JavaScript instance
// we also maintain a secondary reference count.  When that reference count reaches
// 1 OR the applet is shutdown we deref the JavaScript instance.  Applet instances
// are represented by a jlong.

typedef HashMap<const RootObject*, ProtectCountSet*> RootObjectMap;

static RootObjectMap* rootObjectMap()
{
    static RootObjectMap staticRootObjectMap;
    return &staticRootObjectMap;
}

static ProtectCountSet* getProtectCountSet(const RootObject* rootObject)
{
    ProtectCountSet* protectCountSet = rootObjectMap()->get(rootObject);

    if (!protectCountSet) {
        protectCountSet = new ProtectCountSet;
        rootObjectMap()->add(rootObject, protectCountSet);
    }
    return protectCountSet;
}

static void destroyProtectCountSet(const RootObject* rootObject, ProtectCountSet* protectCountSet)
{
    rootObjectMap()->remove(rootObject);
    delete protectCountSet;
}

// FIXME:  These two functions are a potential performance problem.  We could 
// fix them by adding a JSObject to RootObject dictionary.

ProtectCountSet* findProtectCountSet(JSObject* jsObject)
{
    const RootObject* rootObject = findRootObject(jsObject);
    return rootObject ? getProtectCountSet(rootObject) : 0;
}

const RootObject* findRootObject(JSObject* jsObject)
{
    RootObjectMap::const_iterator end = rootObjectMap()->end();
    for (RootObjectMap::const_iterator it = rootObjectMap()->begin(); it != end; ++it) {
        ProtectCountSet* set = it->second;
        if (set->contains(jsObject))
            return it->first;
    }
    
    return 0;
}

const RootObject* findRootObject(Interpreter* interpreter)
{
    RootObjectMap::const_iterator end = rootObjectMap()->end();
    for (RootObjectMap::const_iterator it = rootObjectMap()->begin(); it != end; ++it) {
        const RootObject* aRootObject = it->first;
        
        if (aRootObject->interpreter() == interpreter)
            return aRootObject;
    }
    
    return 0;
}

void addNativeReference(const RootObject* rootObject, JSObject* jsObject)
{
    if (!rootObject)
        return;
    
    ProtectCountSet* protectCountSet = getProtectCountSet(rootObject);
    if (!protectCountSet->contains(jsObject)) {
        JSLock lock;
        gcProtect(jsObject);
    }
    protectCountSet->add(jsObject);
}

void removeNativeReference(JSObject* jsObject)
{
    if (!jsObject)
        return;

    // We might have manually detroyed the root object and its protect set already
    ProtectCountSet* protectCountSet = findProtectCountSet(jsObject);
    if (!protectCountSet)
        return;

    if (protectCountSet->count(jsObject) == 1) {
        JSLock lock;
        gcUnprotect(jsObject);
    }
    protectCountSet->remove(jsObject);
}

#if PLATFORM(MAC)
// May only be set by dispatchToJavaScriptThread().
static CFRunLoopSourceRef completionSource;

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

static void performJavaScriptAccess(void*)
{
    assert (CFRunLoopGetCurrent() == RootObject::runLoop());
    
    // Dispatch JavaScript calls here.
    CFRunLoopSourceContext sourceContext;
    CFRunLoopSourceGetContext (completionSource, &sourceContext);
    JSObjectCallContext *callContext = (JSObjectCallContext *)sourceContext.info;    
    CFRunLoopRef originatingLoop = callContext->originatingLoop;

    JavaJSObject::invoke (callContext);
    
    // Signal the originating thread that we're done.
    CFRunLoopSourceSignal (completionSource);
    if (CFRunLoopIsWaiting(originatingLoop)) {
        CFRunLoopWakeUp(originatingLoop);
    }
}

CreateRootObjectFunction RootObject::_createRootObject = 0;
CFRunLoopRef RootObject::_runLoop = 0;
CFRunLoopSourceRef RootObject::_performJavaScriptSource = 0;

// Must be called from the thread that will be used to access JavaScript.
void RootObject::setCreateRootObject(CreateRootObjectFunction createRootObject) {
    // Should only be called once.
    ASSERT(!_createRootObject);

    _createRootObject = createRootObject;
    
    // Assume that we can retain this run loop forever.  It'll most 
    // likely (always?) be the main loop.
    _runLoop = (CFRunLoopRef)CFRetain (CFRunLoopGetCurrent ());

    // Setup a source the other threads can use to signal the _runLoop
    // thread that a JavaScript call needs to be invoked.
    CFRunLoopSourceContext sourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, performJavaScriptAccess};
    RootObject::_performJavaScriptSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(RootObject::_runLoop, RootObject::_performJavaScriptSource, kCFRunLoopDefaultMode);
}
#endif

// Destroys the RootObject and unprotects all JSObjects associated with it.
void RootObject::destroy()
{
    ProtectCountSet* protectCountSet = getProtectCountSet(this);
    
    if (protectCountSet) {
        ProtectCountSet::iterator end = protectCountSet->end();
        for (ProtectCountSet::iterator it = protectCountSet->begin(); it != end; ++it) {
            JSLock lock;
            gcUnprotect(it->first);            
        }

        destroyProtectCountSet(this, protectCountSet);
    }

    delete this;
}

} } // namespace KJS::Bindings
