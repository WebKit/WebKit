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

#import "DOMImplementationInternal.h"

#import "DOMCSSStyleSheetInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMDocumentTypeInternal.h"
#import "DOMHTMLDocumentInternal.h"
#import "DOMInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/CSSStyleSheet.h>
#import <WebCore/DOMImplementation.h>
#import <WebCore/DocumentType.h>
#import <WebCore/HTMLDocument.h>
#import <WebCore/JSExecState.h>
#import <WebCore/SVGTests.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>

@implementation DOMImplementation

static inline WebCore::DOMImplementation& unwrap(DOMImplementation& wrapper)
{
    return *reinterpret_cast<WebCore::DOMImplementation*>(wrapper._internal);
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([DOMImplementation class], self))
        return;
    if (_internal)
        unwrap(*self).deref();
    [super dealloc];
}

- (BOOL)hasFeature:(NSString *)feature version:(NSString *)version
{
    return WebCore::SVGTests::hasFeatureForLegacyBindings(feature, version);
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName publicId:(NSString *)publicId systemId:(NSString *)systemId
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).createDocumentType(qualifiedName, publicId, systemId)).ptr());
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName doctype:(DOMDocumentType *)doctype
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(unwrap(*self).createDocument(namespaceURI, qualifiedName, core(doctype))).ptr());
}

- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title media:(NSString *)media
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).createCSSStyleSheet(title, media).ptr());
}

- (DOMHTMLDocument *)createHTMLDocument:(NSString *)title
{
    WebCore::JSMainThreadNullState state;
    return kit(unwrap(*self).createHTMLDocument(title).ptr());
}

@end

@implementation DOMImplementation (DOMImplementationDeprecated)

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version
{
    return [self hasFeature:feature version:version];
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId
{
    return [self createDocumentType:qualifiedName publicId:publicId systemId:systemId];
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype
{
    return [self createDocument:namespaceURI qualifiedName:qualifiedName doctype:doctype];
}

- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title :(NSString *)media
{
    return [self createCSSStyleSheet:title media:media];
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
