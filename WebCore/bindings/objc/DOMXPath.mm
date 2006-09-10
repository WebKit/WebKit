/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#ifdef XPATH_SUPPORT

#import "DOMXPath.h"

#import "DOMInternal.h"
#import "Document.h"
#import "XPathExpression.h"
#import "XPathNSResolver.h"
#import "XPathResult.h"

using WebCore::ExceptionCode;
using WebCore::XPathExpression;
using WebCore::XPathNSResolver;
using WebCore::XPathResult;

//------------------------------------------------------------------------------------------
// DOMNativeXPathNSResolver

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


//------------------------------------------------------------------------------------------
// DOMXPathResult

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


//------------------------------------------------------------------------------------------
// DOMXPathResult

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

#endif // XPATH_SUPPORT
