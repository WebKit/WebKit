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

#import "DOMInternal.h"

#import <dom/dom2_range.h>
#import <dom/dom_exception.h>
#import <dom/dom_string.h>
#import <xml/dom_docimpl.h>
#import <xml/dom_stringimpl.h>

#import "kjs_proxy.h"

#import "KWQAssertions.h"
#import "KWQKHTMLPart.h"

#import <JavaScriptCore/interpreter.h>
#import <JavaScriptCore/runtime_root.h>
#import <JavaScriptCore/WebScriptObjectPrivate.h>

//#import "kjs_dom.h"
// We can't include kjs_dom.h here because of ObjC and C++ namespace compiler craziness.
// kjs_dom.h:169: error: statically allocated instance of Objective-C class `DOMImplementation'
// Declaring getDOMNode directly instead.
namespace KJS {
    Value getDOMNode(ExecState *exec, const DOM::Node &n);
}

using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::RangeException;

//------------------------------------------------------------------------------------------
// Wrapping khtml implementation objects

static CFMutableDictionaryRef wrapperCache = NULL;

id getDOMWrapperImpl(DOMObjectInternal *impl)
{
    if (!wrapperCache)
        return nil;
    return (id)CFDictionaryGetValue(wrapperCache, impl);
}

void addDOMWrapperImpl(id wrapper, DOMObjectInternal *impl)
{
    if (!wrapperCache) {
        // No need to retain/free either impl key, or id value.  Items will be removed
        // from the cache in dealloc methods.
        wrapperCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    CFDictionarySetValue(wrapperCache, impl, wrapper);
}

void removeDOMWrapper(DOMObjectInternal *impl)
{
    if (!wrapperCache)
        return;
    CFDictionaryRemoveValue(wrapperCache, impl);
}

//------------------------------------------------------------------------------------------
// Exceptions

NSString * const DOMException = @"DOMException";
NSString * const DOMRangeException = @"DOMRangeException";

void raiseDOMException(int code)
{
    ASSERT(code);
    
    NSString *name;
    if (code >= RangeException::_EXCEPTION_OFFSET) {
        name = DOMRangeException;
        code -= RangeException::_EXCEPTION_OFFSET;
    }
    else {
        name = DOMException;
    }
    NSString *reason = [NSString stringWithFormat:@"*** Exception received from DOM API: %d", code];
    NSException *exception = [NSException exceptionWithName:name reason:reason
        userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:code] forKey:name]];
    [exception raise];
}

//------------------------------------------------------------------------------------------
// DOMString/NSString bridging

DOMString::operator NSString *() const
{
    return [NSString stringWithCharacters:reinterpret_cast<const unichar *>(unicode()) length:length()];
}

DOMString::DOMString(NSString *str)
{
    ASSERT(str);

    CFIndex size = CFStringGetLength(reinterpret_cast<CFStringRef>(str));
    if (size == 0)
        impl = DOMStringImpl::empty();
    else {
        UniChar fixedSizeBuffer[1024];
        UniChar *buffer;
        if (size > static_cast<CFIndex>(sizeof(fixedSizeBuffer) / sizeof(UniChar))) {
            buffer = static_cast<UniChar *>(malloc(size * sizeof(UniChar)));
        } else {
            buffer = fixedSizeBuffer;
        }
        CFStringGetCharacters(reinterpret_cast<CFStringRef>(str), CFRangeMake(0, size), buffer);
        impl = new DOMStringImpl(reinterpret_cast<const QChar *>(buffer), (uint)size);
        if (buffer != fixedSizeBuffer) {
            free(buffer);
        }
    }
    impl->ref();
}

//------------------------------------------------------------------------------------------

@implementation WebScriptObject (WebScriptObjectInternal)

// Only called by DOMObject subclass.
- _init
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
    assert (_private->isCreatedByDOMWrapper);
    
    if (![self isKindOfClass:[DOMNode class]]) {
        // DOMObject can't map back to a document, and thus an interpreter,
        // so for now only create wrappers for DOMNodes.
        NSLog (@"%s:%d:  We don't know how to create ObjC JS wrappers from DOMObjects yet.", __FILE__, __LINE__);
        return;
    }
    
    // Extract the DOM::NodeImpl from the ObjectiveC wrapper.
    DOMNode *n = (DOMNode *)self;
    DOM::NodeImpl *nodeImpl = [n _nodeImpl];

    // Dig up Interpreter and ExecState.
    KHTMLPart *part = nodeImpl->getDocument()->part();
    KJS::Interpreter *interpreter = KJSProxy::proxy(part)->interpreter();
    KJS::ExecState *exec = interpreter->globalExec();
    
    // Get (or create) a cached JS object for the DOM node.
    KJS::ObjectImp *scriptImp = static_cast<KJS::ObjectImp *>(KJS::getDOMNode (exec, DOM::Node (nodeImpl)).imp());

    const KJS::Bindings::RootObject *executionContext = KWQ(part)->bindingRootObject();
    [self _initializeWithObjectImp:scriptImp originExecutionContext:executionContext executionContext:executionContext];
}

@end


