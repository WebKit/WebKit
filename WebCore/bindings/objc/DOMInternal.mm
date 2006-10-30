/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "FrameMac.h"
#import "Node.h"
#import "PlatformString.h"
#import "Range.h"
#import "RangeException.h"
#import "SVGException.h"
#import "WebScriptObjectPrivate.h"
#import "XPathEvaluator.h"
#import "kjs_dom.h"
#import "kjs_proxy.h"

//------------------------------------------------------------------------------------------
// Wrapping WebCore implementation objects

namespace WebCore {

static HashMap<DOMObjectInternal*, NSObject*>* wrapperCache;

NSObject* getDOMWrapper(DOMObjectInternal* impl)
{
    if (!wrapperCache)
        return nil;
    return wrapperCache->get(impl);
}

void addDOMWrapper(NSObject* wrapper, DOMObjectInternal* impl)
{
    if (!wrapperCache)
        wrapperCache = new HashMap<DOMObjectInternal*, NSObject*>;
    wrapperCache->set(impl, wrapper);
}

void removeDOMWrapper(DOMObjectInternal* impl)
{
    if (!wrapperCache)
        return;
    wrapperCache->remove(impl);
}

} // namespace WebCore


//------------------------------------------------------------------------------------------
// Exceptions

NSString * const DOMException = @"DOMException";
NSString * const DOMRangeException = @"DOMRangeException";
NSString * const DOMEventException = @"DOMEventException";
#ifdef SVG_SUPPORT
NSString * const DOMSVGException = @"DOMSVGException";
#endif // SVG_SUPPORT
#ifdef XPATH_SUPPORT
NSString * const DOMXPathException = @"DOMXPathException";
#endif // XPATH_SUPPORT

namespace WebCore {

void raiseDOMException(ExceptionCode ec)
{
    ASSERT(ec);

    NSString *name = ::DOMException;

    int code = ec;
    if (ec >= RangeExceptionOffset && ec <= RangeExceptionMax) {
        name = DOMRangeException;
        code -= RangeExceptionOffset;
    } else if (ec >= EventExceptionOffset && ec <= EventExceptionMax) {
        name = DOMEventException;
        code -= EventExceptionOffset;
#ifdef SVG_SUPPORT
    } else if (ec >= SVGExceptionOffset && ec <= SVGExceptionMax) {
        name = DOMSVGException;
        code -= SVGExceptionOffset;
#endif // SVG_SUPPORT
#ifdef XPATH_SUPPORT
    } else if (ec >= XPathExceptionOffset && ec <= XPathExceptionMax) {
        name = DOMXPathException;
        code -= XPathExceptionOffset;
#endif // XPATH_SUPPORT
    }

    NSString *reason = [NSString stringWithFormat:@"*** Exception received from DOM API: %d", code];
    NSException *exception = [NSException exceptionWithName:name reason:reason
        userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:code] forKey:name]];
    [exception raise];
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
    WebCore::Frame *frame = nodeImpl->document()->frame();
    KJS::Interpreter *interpreter = frame->scriptProxy()->interpreter();
    KJS::ExecState *exec = interpreter->globalExec();
    
    // Get (or create) a cached JS object for the DOM node.
    KJS::JSObject *scriptImp = static_cast<KJS::JSObject*>(KJS::toJS(exec, nodeImpl));

    const KJS::Bindings::RootObject *executionContext = WebCore::Mac(frame)->bindingRootObject();

    [self _initializeWithObjectImp:scriptImp originExecutionContext:executionContext executionContext:executionContext];
}

@end

namespace WebCore {

NSString* displayString(const String& string, const Node* node)
{
    if (!node)
        return string;
    Document* document = node->document();
    if (!document)
        return string;
    String copy(string);
    copy.replace('\\', document->backslashAsCurrencySymbol());
    return copy;
}

} // namespace WebCore
