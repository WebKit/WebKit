/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc.  All rights reserved.
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

#import "config.h"
#import "DOMInternal.h"

#import "Document.h"
#import "Event.h"
#import "Frame.h"
#import "JSNode.h"
#import "Node.h"
#import "PlatformString.h"
#import "Range.h"
#import "RangeException.h"
#import "SVGException.h"
#import "WebScriptObjectPrivate.h"
#import "XPathEvaluator.h"
#import "kjs_proxy.h"

//------------------------------------------------------------------------------------------
// Wrapping WebCore implementation objects

namespace WebCore {

typedef HashMap<DOMObjectInternal*, NSObject*> DOMWrapperMap;
static DOMWrapperMap* DOMWrapperCache;

NSObject* getDOMWrapper(DOMObjectInternal* impl)
{
    if (!DOMWrapperCache)
        return nil;
    return DOMWrapperCache->get(impl);
}

void addDOMWrapper(NSObject* wrapper, DOMObjectInternal* impl)
{
    if (!DOMWrapperCache)
        DOMWrapperCache = new DOMWrapperMap;
    DOMWrapperCache->set(impl, wrapper);
}

void removeDOMWrapper(DOMObjectInternal* impl)
{
    if (!DOMWrapperCache)
        return;
    DOMWrapperCache->remove(impl);
}

} // namespace WebCore


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
    assert (_private->isCreatedByDOMWrapper);
    
    if (![self isKindOfClass:[DOMNode class]]) {
        // DOMObject can't map back to a document, and thus an interpreter,
        // so for now only create wrappers for DOMNodes.
        NSLog(@"%s:%d:  We don't know how to create ObjC JS wrappers from DOMObjects yet.", __FILE__, __LINE__);
        return;
    }
    
    // Extract the WebCore::Node from the ObjectiveC wrapper.
    DOMNode *n = (DOMNode *)self;
    WebCore::Node *nodeImpl = [n _node];

    // Dig up Interpreter and ExecState.
    WebCore::Frame *frame = 0;
    if (WebCore::Document* document = nodeImpl->document())
        frame = document->frame();
    if (!frame)
        return;
        
    KJS::ExecState *exec = frame->scriptProxy()->globalObject()->globalExec();
    
    // Get (or create) a cached JS object for the DOM node.
    KJS::JSObject *scriptImp = static_cast<KJS::JSObject*>(WebCore::toJS(exec, nodeImpl));

    KJS::Bindings::RootObject* rootObject = frame->bindingRootObject();

    [self _setImp:scriptImp originRootObject:rootObject rootObject:rootObject];
}

@end
