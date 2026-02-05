/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKDOMDocument.h"

#import "WKDOMInternals.h"
#import <WebCore/Document.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/SimpleRange.h>
#import <WebCore/Text.h>
#import <WebCore/markup.h>

@interface WKDOMDocumentParserYieldToken : NSObject

@end

@implementation WKDOMDocumentParserYieldToken {
    std::unique_ptr<WebCore::DocumentParserYieldToken> _token;
}

- (instancetype)initWithDocument:(std::reference_wrapper<WebCore::Document>)document
{
    if (self = [super init])
        _token = Ref { document.get() }->createParserYieldToken();
    return self;
}

@end

static Ref<WebCore::Document> protectedImpl(WKDOMDocument *document)
{
    return downcast<WebCore::Document>(*document->_impl);
}

@implementation WKDOMDocument

- (WKDOMElement *)createElement:(NSString *)tagName
{
    // FIXME: Do something about the exception.
    auto result = protectedImpl(self)->createElementForBindings(tagName);
    if (result.hasException())
        return nil;
    return WebKit::toWKDOMElement(result.releaseReturnValue().ptr());
}

- (WKDOMText *)createTextNode:(NSString *)data
{
    return WebKit::toWKDOMText(protectedImpl(self)->createTextNode(data).ptr());
}

- (WKDOMElement *)body
{
    return WebKit::toWKDOMElement(protect(protectedImpl(self)->bodyOrFrameset()).get());
}

- (WKDOMNode *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL
{
    return WebKit::toWKDOMNode(createFragmentFromMarkup(protectedImpl(self), markupString, baseURL.absoluteString).ptr());
}

- (WKDOMNode *)createDocumentFragmentWithText:(NSString *)text
{
    return WebKit::toWKDOMNode(createFragmentFromText(makeRangeSelectingNodeContents(protectedImpl(self)), text).ptr());
}

- (id)parserYieldToken
{
    return adoptNS([[WKDOMDocumentParserYieldToken alloc] initWithDocument:protectedImpl(self)]).autorelease();
}

@end
