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

typedef HashMap<const RootObject*, ReferencesSet*> ReferencesByRootMap; 

static ReferencesByRootMap* getReferencesByRootMap()
{
    static ReferencesByRootMap* referencesByRootMap = 0;
    
    if (!referencesByRootMap)
        referencesByRootMap = new ReferencesByRootMap;
    
    return referencesByRootMap;
}

static ReferencesSet* getReferencesSet(const RootObject* rootObject)
{
    ReferencesByRootMap* refsByRoot = getReferencesByRootMap();
    ReferencesSet* referencesSet = 0;
    
    referencesSet = refsByRoot->get(rootObject);
    if (!referencesSet) {
        referencesSet  = new ReferencesSet;
        refsByRoot->add(rootObject, referencesSet);
    }
    return referencesSet;
}

// Scan all the dictionary for all the roots to see if any have a 
// reference to the imp, and if so, return it's reference count
// dictionary.
// FIXME:  This is a potential performance bottleneck with many applets.  We could fix be adding a
// imp to root dictionary.
ReferencesSet* findReferenceSet(JSObject* imp)
{
    ReferencesByRootMap* refsByRoot = getReferencesByRootMap ();
    if (refsByRoot) {
        ReferencesByRootMap::const_iterator end = refsByRoot->end();
        for (ReferencesByRootMap::const_iterator it = refsByRoot->begin(); it != end; ++it) {
            ReferencesSet* set = it->second;
            
            if (set->contains(imp))
                return set;
        }
    }
    
    return 0;
}

// FIXME:  This is a potential performance bottleneck with many applets.  We could fix be adding a
// imp to root dictionary.
const RootObject* rootObjectForImp (JSObject* imp)
{
    ReferencesByRootMap* refsByRoot = getReferencesByRootMap ();
    const RootObject* rootObject = 0;
    
    if (refsByRoot) {
        ReferencesByRootMap::const_iterator end = refsByRoot->end();
        for (ReferencesByRootMap::const_iterator it = refsByRoot->begin(); it != end; ++it) {
            ReferencesSet* set = it->second;
            if (set->contains(imp)) {
                rootObject = it->first;
                break;
            }
        }
    }
    
    return rootObject;
}

const RootObject* rootObjectForInterpreter(Interpreter* interpreter)
{
    ReferencesByRootMap* refsByRoot = getReferencesByRootMap ();
    
    if (refsByRoot) {
        ReferencesByRootMap::const_iterator end = refsByRoot->end();
        for (ReferencesByRootMap::const_iterator it = refsByRoot->begin(); it != end; ++it) {
            const RootObject* aRootObject = it->first;
            
            if (aRootObject->interpreter() == interpreter)
                return aRootObject;
        }
    }
    
    return 0;
}

void addNativeReference(const RootObject* rootObject, JSObject* imp)
{
    if (rootObject) {
        ReferencesSet* referenceMap = getReferencesSet(rootObject);
        
        unsigned numReferences = referenceMap->count(imp);
        if (numReferences == 0) {
            JSLock lock;
            gcProtect(imp);
        }
        referenceMap->add(imp);
    }
}

void removeNativeReference(JSObject* imp)
{
    if (!imp)
        return;

    ReferencesSet *referencesSet = findReferenceSet(imp);
    if (referencesSet) {
        unsigned numReferences = referencesSet->count(imp);
        
        if (numReferences == 1) {
            JSLock lock;
            gcUnprotect(imp);
        }
        referencesSet->remove(imp);
    }
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
    RootObject::_performJavaScriptSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(RootObject::_runLoop, RootObject::_performJavaScriptSource, kCFRunLoopDefaultMode);
}
#endif
// Must be called when the applet is shutdown.
void RootObject::removeAllNativeReferences ()
{
    ReferencesSet* referencesSet = getReferencesSet (this);
    
    if (referencesSet) {
        ReferencesSet::iterator end = referencesSet->end();
        for (ReferencesSet::iterator it = referencesSet->begin(); it != end; ++it) {
            JSLock lock;
            gcUnprotect(it->first);            
        }
        referencesSet->clear();
        ReferencesByRootMap* refsByRoot = getReferencesByRootMap();
        refsByRoot->remove(this);
        delete referencesSet;
        delete this;
    }
}

void RootObject::setInterpreter (Interpreter* interpreter)
{
    _interpreter = interpreter;
}

} } // namespace KJS::Bindings
