/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#if XPATH_SUPPORT

#import "DOMXPath.h"
#import "DOMInternal.h"
#import "DOMXPathInternal.h"
#import "Document.h"

using WebCore::ExceptionCode;
using WebCore::XPathExpression;
using WebCore::XPathNSResolver;
using WebCore::XPathResult;

@implementation DOMNativeXPathNSResolver

- (void)dealloc
{
    if (_internal) {
        DOM_cast<XPathNSResolver*>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<XPathNSResolver*>(_internal)->deref();
    }
    [super finalize];
}

- (XPathNSResolver *)_xpathNSResolver
{
    return DOM_cast<XPathNSResolver *>(_internal);
}

- (id)_initWithXPathNSResolver:(XPathNSResolver *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;    
}

+ (DOMNativeXPathNSResolver *)_xpathNSResolverWith:(XPathNSResolver *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[DOMNativeXPathNSResolver alloc] _initWithXPathNSResolver:impl] autorelease];    
}

- (NSString *)lookupNamespaceURI:(NSString *)prefix
{
    return [self _xpathNSResolver]->lookupNamespaceURI(prefix);
}

@end

@implementation DOMXPathResult

- (void)dealloc
{
    if (_internal) {
        DOM_cast<XPathResult*>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<XPathResult*>(_internal)->deref();
    }
    [super finalize];
}

- (unsigned short)resultType
{
    return [self _xpathResult]->resultType();
}

- (double)numberValue
{
    ExceptionCode ec = 0;
    double result = [self _xpathResult]->numberValue(ec);
    raiseOnDOMError(ec);
    return result;
}

- (NSString *)stringValue
{
    ExceptionCode ec = 0;
    NSString *result = [self _xpathResult]->stringValue(ec);
    raiseOnDOMError(ec);
    return result;
}

- (BOOL)booleanValue
{
    ExceptionCode ec = 0;
    BOOL result = [self _xpathResult]->booleanValue(ec);
    raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)singleNodeValue
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _xpathResult]->singleNodeValue(ec)];
    raiseOnDOMError(ec);
    return result;    
}

- (BOOL)invalidIteratorState
{
    return [self _xpathResult]->invalidIteratorState();
}

- (unsigned)snapshotLength
{
    ExceptionCode ec = 0;
    unsigned result = [self _xpathResult]->snapshotLength(ec);
    raiseOnDOMError(ec);
    return result;        
}

- (DOMNode *)iterateNext
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _xpathResult]->iterateNext(ec)];
    raiseOnDOMError(ec);
    return result;    
}

- (DOMNode *)snapshotItem:(unsigned)index
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _xpathResult]->snapshotItem(index, ec)];
    raiseOnDOMError(ec);
    return result;    
}

@end

@implementation DOMXPathResult (WebCoreInternal)

- (XPathResult *)_xpathResult
{
    return DOM_cast<XPathResult *>(_internal);
}

- (id)_initWithXPathResult:(XPathResult *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMXPathResult *)_xpathResultWith:(XPathResult *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[DOMXPathResult alloc] _initWithXPathResult:impl] autorelease];
}

@end


@implementation DOMXPathExpression

- (void)dealloc
{
    if (_internal) {
        DOM_cast<XPathNSResolver*>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<XPathNSResolver*>(_internal)->deref();
    }
    [super finalize];
}

- (DOMXPathResult *)evaluate:(DOMNode *)contextNode :(unsigned short)type :(DOMXPathResult *)result
{
    ExceptionCode ec = 0;
    DOMXPathResult *_result = [DOMXPathResult _xpathResultWith:[self _xpathExpression]->evaluate([contextNode _node], type, [result _xpathResult], ec).get()];
    raiseOnDOMError(ec);
    return _result;
}

@end

@implementation DOMXPathExpression (WebCoreInternal)

- (XPathExpression *)_xpathExpression
{
    return DOM_cast<XPathExpression *>(_internal);
}

- (id)_initWithXPathExpression:(XPathExpression *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMXPathExpression *)_xpathExpressionWith:(XPathExpression *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[DOMXPathExpression alloc] _initWithXPathExpression:impl] autorelease];
}

@end

@implementation DOMDocument (DOMDocumentXPath)

- (DOMXPathExpression *)createExpression:(NSString *)expression :(id <DOMXPathNSResolver>)resolver
{
    if (resolver && ![resolver isMemberOfClass:[DOMNativeXPathNSResolver class]])
        [NSException raise:NSGenericException format:@"createExpression currently does not work with custom NS resolvers"];
    
    DOMNativeXPathNSResolver *nativeResolver = (DOMNativeXPathNSResolver *)resolver;
    ExceptionCode ec = 0;
    DOMXPathExpression *result = [DOMXPathExpression _xpathExpressionWith:[self _document]->createExpression(expression, [nativeResolver _xpathNSResolver], ec).get()];
    raiseOnDOMError(ec);
    return result;
}

- (id <DOMXPathNSResolver>)createNSResolver:(DOMNode *)nodeResolver
{
    return [DOMNativeXPathNSResolver _xpathNSResolverWith:[self _document]->createNSResolver([nodeResolver _node]).get()];
}

- (DOMXPathResult *)evaluate:(NSString *)expression :(DOMNode *)contextNode :(id <DOMXPathNSResolver>)resolver :(unsigned short)type :(DOMXPathResult *)result
{
    if (resolver && ![resolver isMemberOfClass:[DOMNativeXPathNSResolver class]])
        [NSException raise:NSGenericException format:@"createExpression currently does not work with custom NS resolvers"];
    
    DOMNativeXPathNSResolver *nativeResolver = (DOMNativeXPathNSResolver *)resolver;
    ExceptionCode ec = 0;
    DOMXPathResult *_result = [DOMXPathResult _xpathResultWith:[self _document]->evaluate(expression, [contextNode _node], [nativeResolver _xpathNSResolver], type, [result _xpathResult], ec).get()];
    raiseOnDOMError(ec);
    return _result;
}

@end

#endif // XPATH_SUPPORT
