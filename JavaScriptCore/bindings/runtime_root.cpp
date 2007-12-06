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
#include "runtime_root.h"

#include "JSGlobalObject.h"
#include "object.h"
#include "runtime.h"
#include "runtime_object.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>

namespace KJS { namespace Bindings {

// This code attempts to solve two problems: (1) plug-ins leaking references to 
// JS and the DOM; (2) plug-ins holding stale references to JS and the DOM. Previous 
// comments in this file claimed that problem #1 was an issue in Java, in particular, 
// because Java, allegedly, didn't always call finalize when collecting an object.

typedef HashSet<RootObject*> RootObjectSet;

static RootObjectSet* rootObjectSet()
{
    static RootObjectSet staticRootObjectSet;
    return &staticRootObjectSet;
}

// FIXME:  These two functions are a potential performance problem.  We could 
// fix them by adding a JSObject to RootObject dictionary.

RootObject* findProtectingRootObject(JSObject* jsObject)
{
    RootObjectSet::const_iterator end = rootObjectSet()->end();
    for (RootObjectSet::const_iterator it = rootObjectSet()->begin(); it != end; ++it) {
        if ((*it)->gcIsProtected(jsObject))
            return *it;
    }
    return 0;
}

RootObject* findRootObject(JSGlobalObject* globalObject)
{
    RootObjectSet::const_iterator end = rootObjectSet()->end();
    for (RootObjectSet::const_iterator it = rootObjectSet()->begin(); it != end; ++it) {
        if ((*it)->globalObject() == globalObject)
            return *it;
    }
    return 0;
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

PassRefPtr<RootObject> RootObject::create(const void* nativeHandle, JSGlobalObject* globalObject)
{
    return new RootObject(nativeHandle, globalObject);
}

RootObject::RootObject(const void* nativeHandle, JSGlobalObject* globalObject)
    : m_isValid(true)
    , m_nativeHandle(nativeHandle)
    , m_globalObject(globalObject)
{
    ASSERT(globalObject);
    rootObjectSet()->add(this);
}

RootObject::~RootObject()
{
    if (m_isValid)
        invalidate();
}

void RootObject::invalidate()
{
    if (!m_isValid)
        return;

    {
        HashSet<RuntimeObjectImp*>::iterator end = m_runtimeObjects.end();
        for (HashSet<RuntimeObjectImp*>::iterator it = m_runtimeObjects.begin(); it != end; ++it)
            (*it)->invalidate();
        
        m_runtimeObjects.clear();
    }
    
    m_isValid = false;

    m_nativeHandle = 0;
    m_globalObject = 0;

    ProtectCountSet::iterator end = m_protectCountSet.end();
    for (ProtectCountSet::iterator it = m_protectCountSet.begin(); it != end; ++it) {
        JSLock lock;
        KJS::gcUnprotect(it->first);
    }
    m_protectCountSet.clear();

    rootObjectSet()->remove(this);
}

void RootObject::gcProtect(JSObject* jsObject)
{
    ASSERT(m_isValid);
    
    if (!m_protectCountSet.contains(jsObject)) {
        JSLock lock;
        KJS::gcProtect(jsObject);
    }
    m_protectCountSet.add(jsObject);
}

void RootObject::gcUnprotect(JSObject* jsObject)
{
    ASSERT(m_isValid);
    
    if (!jsObject)
        return;

    if (m_protectCountSet.count(jsObject) == 1) {
        JSLock lock;
        KJS::gcUnprotect(jsObject);
    }
    m_protectCountSet.remove(jsObject);
}

bool RootObject::gcIsProtected(JSObject* jsObject)
{
    ASSERT(m_isValid);
    return m_protectCountSet.contains(jsObject);
}

const void* RootObject::nativeHandle() const 
{ 
    ASSERT(m_isValid);
    return m_nativeHandle; 
}

JSGlobalObject* RootObject::globalObject() const
{
    ASSERT(m_isValid);
    return m_globalObject;
}

void RootObject::addRuntimeObject(RuntimeObjectImp* object)
{
    ASSERT(m_isValid);
    ASSERT(!m_runtimeObjects.contains(object));
    
    m_runtimeObjects.add(object);
}        
    
void RootObject::removeRuntimeObject(RuntimeObjectImp* object)
{
    ASSERT(m_isValid);
    ASSERT(m_runtimeObjects.contains(object));
    
    m_runtimeObjects.remove(object);
}

} } // namespace KJS::Bindings
