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

- (WebCore::HTMLDocument *)_HTMLDocument
{
    return static_cast<WebCore::HTMLDocument *>(DOM_cast<WebCore::Node *>(_internal));
}

- (NSString *)title
{
    return [self _HTMLDocument]->title();
}

- (void)setTitle:(NSString *)title
{
    [self _HTMLDocument]->setTitle(title);
}

- (NSString *)referrer
{
     return [self _HTMLDocument]->referrer();
}

- (NSString *)domain
{
     return [self _HTMLDocument]->domain();
}

- (NSString *)URL
{
    return [self _HTMLDocument]->URL().getNSString();
}

- (DOMHTMLElement *)body
{
    return [DOMHTMLElement _HTMLElementWith:[self _HTMLDocument]->body()];
}

- (void)setBody:(DOMHTMLElement *)body
{
    WebCore::ExceptionCode ec = 0;
    [self _HTMLDocument]->setBody([body _HTMLElement], ec);
    raiseOnDOMError(ec);
}

- (DOMHTMLCollection *)images
{
    return [DOMHTMLCollection _HTMLCollectionWith:[self _HTMLDocument]->images().get()];
}

- (DOMHTMLCollection *)applets
{
    return [DOMHTMLCollection _HTMLCollectionWith:[self _HTMLDocument]->applets().get()];
}

- (DOMHTMLCollection *)links
{
    return [DOMHTMLCollection _HTMLCollectionWith:[self _HTMLDocument]->links().get()];
}

- (DOMHTMLCollection *)forms
{
    return [DOMHTMLCollection _HTMLCollectionWith:[self _HTMLDocument]->forms().get()];
}

- (DOMHTMLCollection *)anchors
{
    return [DOMHTMLCollection _HTMLCollectionWith:[self _HTMLDocument]->anchors().get()];
}

- (NSString *)cookie
{
    return [self _HTMLDocument]->cookie();
}

- (void)setCookie:(NSString *)cookie
{
    [self _HTMLDocument]->setCookie(cookie);
}

- (void)open
{
    [self _HTMLDocument]->open();
}

- (void)close
{
    [self _HTMLDocument]->close();
}

- (void)write:(NSString *)text
{
    [self _HTMLDocument]->write(text);
}

- (void)writeln:(NSString *)text
{
    [self _HTMLDocument]->writeln(text);
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    return [DOMElement _elementWith:[self _HTMLDocument]->getElementById(elementId)];
}

- (DOMNodeList *)getElementsByName:(NSString *)elementName
{
    return [DOMNodeList _nodeListWith:[self _HTMLDocument]->getElementsByName(elementName).get()];
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
