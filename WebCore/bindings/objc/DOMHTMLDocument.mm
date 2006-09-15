/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "DOMHTMLDocument.h"

#import "DOMInternal.h"
#import "HTMLDocument.h" // implementation class

#import "DOMDocumentFragment.h"
#import "DOMHTMLCollection.h"
#import "DOMHTMLElement.h"
#import "DOMNodeList.h"
#import "DocumentFragment.h"
#import "HTMLCollection.h"
#import "NameNodeList.h"
#import "Range.h"
#import "markup.h"

@implementation DOMHTMLDocument

#define IMPL static_cast<WebCore::HTMLDocument*>(reinterpret_cast<WebCore::Node*>(_internal))

- (NSString *)title
{
    return IMPL->title();
}

- (void)setTitle:(NSString *)title
{
    IMPL->setTitle(title);
}

- (NSString *)referrer
{
     return IMPL->referrer();
}

- (NSString *)domain
{
     return IMPL->domain();
}

- (NSString *)URL
{
    return IMPL->URL().getNSString();
}

- (DOMHTMLElement *)body
{
    return [DOMHTMLElement _HTMLElementWith:IMPL->body()];
}

- (void)setBody:(DOMHTMLElement *)body
{
    WebCore::ExceptionCode ec = 0;
    IMPL->setBody([body _HTMLElement], ec);
    raiseOnDOMError(ec);
}

- (DOMHTMLCollection *)images
{
    return [DOMHTMLCollection _HTMLCollectionWith:IMPL->images().get()];
}

- (DOMHTMLCollection *)applets
{
    return [DOMHTMLCollection _HTMLCollectionWith:IMPL->applets().get()];
}

- (DOMHTMLCollection *)links
{
    return [DOMHTMLCollection _HTMLCollectionWith:IMPL->links().get()];
}

- (DOMHTMLCollection *)forms
{
    return [DOMHTMLCollection _HTMLCollectionWith:IMPL->forms().get()];
}

- (DOMHTMLCollection *)anchors
{
    return [DOMHTMLCollection _HTMLCollectionWith:IMPL->anchors().get()];
}

- (NSString *)cookie
{
    return IMPL->cookie();
}

- (void)setCookie:(NSString *)cookie
{
    IMPL->setCookie(cookie);
}

- (void)open
{
    IMPL->open();
}

- (void)close
{
    IMPL->close();
}

- (void)write:(NSString *)text
{
    IMPL->write(text);
}

- (void)writeln:(NSString *)text
{
    IMPL->writeln(text);
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    return [DOMElement _elementWith:IMPL->getElementById(elementId)];
}

- (DOMNodeList *)getElementsByName:(NSString *)elementName
{
    return [DOMNodeList _nodeListWith:IMPL->getElementsByName(elementName).get()];
}

@end

@implementation DOMHTMLDocument (DOMHTMLDocumentExtensions)

- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL
{
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromMarkup([self _document], markupString, [baseURL absoluteString]).get()];
}

- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text
{
    // FIXME: Since this is not a contextual fragment, it won't handle whitespace properly.
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromText([self _document]->createRange().get(), text).get()];
}

@end


@implementation DOMHTMLDocument (WebCoreInternal)

- (WebCore::HTMLDocument *)_HTMLDocument
{
    return IMPL;
}

+ (DOMHTMLDocument *)_HTMLDocumentWith:(WebCore::HTMLDocument *)impl;
{
    return static_cast<DOMHTMLDocument *>([DOMNode _nodeWith:impl]);
}

@end
