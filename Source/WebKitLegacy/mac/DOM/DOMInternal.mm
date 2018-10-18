/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DOMInternal.h"

#import "DOMNodeInternal.h"
#import <WebCore/Document.h>
#import <WebCore/Frame.h>
#import <WebCore/JSNode.h>
#import <WebCore/ScriptController.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/runtime_root.h>
#import <wtf/HashMap.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>

#if PLATFORM(IOS_FAMILY)
#define NEEDS_WRAPPER_CACHE_LOCK 1
#endif

//------------------------------------------------------------------------------------------
// Wrapping WebCore implementation objects

#ifdef NEEDS_WRAPPER_CACHE_LOCK
static Lock wrapperCacheLock;
#endif

static HashMap<DOMObjectInternal*, NSObject *>& wrapperCache()
{
    static NeverDestroyed<HashMap<DOMObjectInternal*, NSObject *>> map;
    return map;
}

NSObject* getDOMWrapper(DOMObjectInternal* impl)
{
#ifdef NEEDS_WRAPPER_CACHE_LOCK
    std::lock_guard<Lock> lock(wrapperCacheLock);
#endif
    return wrapperCache().get(impl);
}

void addDOMWrapper(NSObject* wrapper, DOMObjectInternal* impl)
{
#ifdef NEEDS_WRAPPER_CACHE_LOCK
    std::lock_guard<Lock> lock(wrapperCacheLock);
#endif
    wrapperCache().set(impl, wrapper);
}

void removeDOMWrapper(DOMObjectInternal* impl)
{
#ifdef NEEDS_WRAPPER_CACHE_LOCK
    std::lock_guard<Lock> lock(wrapperCacheLock);
#endif
    wrapperCache().remove(impl);
}

//------------------------------------------------------------------------------------------

@implementation WebScriptObject (WebScriptObjectInternal)

// Only called by DOMObject subclass.
- (id)_init
{
    self = [super init];

    if (![self isKindOfClass:[DOMObject class]]) {
        [NSException raise:NSGenericException format:@"+%@: _init is an internal initializer", [self class]];
        return nil;
    }

    _private = [[WebScriptObjectPrivate alloc] init];
    _private->isCreatedByDOMWrapper = YES;
    
    return self;
}

- (void)_initializeScriptDOMNodeImp
{
    ASSERT(_private->isCreatedByDOMWrapper);
    
    if (![self isKindOfClass:[DOMNode class]]) {
        // DOMObject can't map back to a document, and thus an interpreter,
        // so for now only create wrappers for DOMNodes.
        return;
    }
    
    // Extract the WebCore::Node from the ObjectiveC wrapper.
    DOMNode *n = (DOMNode *)self;
    WebCore::Node *nodeImpl = core(n);

    // Dig up Interpreter and ExecState.
    WebCore::Frame *frame = nodeImpl->document().frame();
    if (!frame)
        return;

    // The global object which should own this node - FIXME: does this need to be isolated-world aware?
    WebCore::JSDOMGlobalObject* globalObject = frame->script().globalObject(WebCore::mainThreadNormalWorld());
    JSC::ExecState *exec = globalObject->globalExec();

    // Get (or create) a cached JS object for the DOM node.
    JSC::JSObject *scriptImp = asObject(WebCore::toJS(exec, globalObject, nodeImpl));

    JSC::Bindings::RootObject* rootObject = frame->script().bindingRootObject();

    [self _setImp:scriptImp originRootObject:rootObject rootObject:rootObject];
}

@end
