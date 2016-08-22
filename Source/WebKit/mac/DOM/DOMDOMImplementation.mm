/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import "DOMDOMImplementationInternal.h"

#import "DOMCSSStyleSheetInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMDocumentTypeInternal.h"
#import "DOMHTMLDocumentInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/CSSStyleSheet.h>
#import <WebCore/DOMImplementation.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentType.h>
#import <WebCore/HTMLDocument.h>
#import <WebCore/JSMainThreadExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/URL.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/XMLDocument.h>
#import <wtf/GetPtr.h>

#define IMPL reinterpret_cast<WebCore::DOMImplementation*>(_internal)

@implementation DOMImplementation

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMImplementation class], self))
        return;

    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (BOOL)hasFeature:(NSString *)feature version:(NSString *)version
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasFeature(feature, version);
}

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasFeature(feature, version);
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName publicId:(NSString *)publicId systemId:(NSString *)systemId
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocumentType *result = kit(WTF::getPtr(IMPL->createDocumentType(qualifiedName, publicId, systemId, ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocumentType *result = kit(WTF::getPtr(IMPL->createDocumentType(qualifiedName, publicId, systemId, ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName doctype:(DOMDocumentType *)doctype
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocument *result = kit(WTF::getPtr(IMPL->createDocument(namespaceURI, qualifiedName, core(doctype), ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMDocument *result = kit(WTF::getPtr(IMPL->createDocument(namespaceURI, qualifiedName, core(doctype), ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title media:(NSString *)media
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMCSSStyleSheet *result = kit(WTF::getPtr(IMPL->createCSSStyleSheet(title, media, ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title :(NSString *)media
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMCSSStyleSheet *result = kit(WTF::getPtr(IMPL->createCSSStyleSheet(title, media, ec)));
    raiseOnDOMError(ec);
    return result;
}

- (DOMHTMLDocument *)createHTMLDocument:(NSString *)title
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createHTMLDocument(title)));
}

@end

DOMImplementation *kit(WebCore::DOMImplementation* value)
{
    WebCoreThreadViolationCheckRoundOne();
    if (!value)
        return nil;
    if (DOMImplementation *wrapper = getDOMWrapper(value))
        return [[wrapper retain] autorelease];
    DOMImplementation *wrapper = [[DOMImplementation alloc] _init];
    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);
    value->ref();
    addDOMWrapper(wrapper, value);
    return [wrapper autorelease];
}
