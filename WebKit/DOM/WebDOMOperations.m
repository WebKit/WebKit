/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebDOMOperationsPrivate.h>

#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebArchiver.h>


@implementation DOMNode (WebDOMNodeOperations)

- (WebFrameBridge *)_bridge
{
    return (WebFrameBridge *)[WebFrameBridge bridgeForDOMDocument:[self ownerDocument]];
}

- (WebArchive *)webArchive
{
    return [WebArchiver archiveNode:self];
}

- (NSString *)markupString
{
    return [[self _bridge] markupStringFromNode:self nodes:nil];
}

- (NSArray *)_URLsFromSelectors:(SEL)firstSel, ...
{
    NSMutableArray *URLs = [NSMutableArray array];
    
    va_list args;
    va_start(args, firstSel);
    
    SEL selector = firstSel;
    do {
        NSString *string = [self performSelector:selector];
        if ([string length] > 0) {
            [URLs addObject:[[self ownerDocument] URLWithAttributeString:string]];
        }
    } while ((selector = va_arg(args, SEL)) != nil);
    
    va_end(args);
    
    return URLs;
}

- (NSArray *)_subresourceURLs
{
    return nil;
}

@end

@implementation DOMDocument (WebDOMDocumentOperations)

- (WebFrame *)webFrame
{
    return [[self _bridge] webFrame];
}

- (NSURL *)URLWithAttributeString:(NSString *)string
{
    return [[self _bridge] URLWithAttributeString:string];
}

@end

@implementation DOMDocument (WebDOMDocumentOperationsPrivate)

- (DOMRange *)_createRangeWithNode:(DOMNode *)node
{
    DOMRange *range = [self createRange];
    [range selectNode:node];
    return range;
}

- (DOMRange *)_documentRange
{
    return [self _createRangeWithNode:[self documentElement]];
}

@end

@implementation DOMRange (WebDOMRangeOperations)

- (WebFrameBridge *)_bridge
{
    return [[self startContainer] _bridge];
}

- (WebArchive *)webArchive
{
    return [WebArchiver archiveRange:self];
}

- (NSString *)markupString
{
    return [[self _bridge] markupStringFromRange:self nodes:nil];
}

@end

@implementation DOMHTMLBodyElement (WebDOMHTMLBodyElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(background), nil];
}

@end

@implementation DOMHTMLInputElement (WebDOMHTMLInputElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(src), nil];
}

@end

@implementation DOMHTMLLinkElement (WebDOMHTMLLinkElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    NSString *rel = [self rel];
    if ([rel _webkit_isCaseInsensitiveEqualToString:@"stylesheet"] || [rel _webkit_isCaseInsensitiveEqualToString:@"icon"]) {
        return [self _URLsFromSelectors:@selector(href), nil];
    }
    return nil;
}

@end

@implementation DOMHTMLScriptElement (WebDOMHTMLScriptElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(src), nil];
}

@end

@implementation DOMHTMLImageElement (WebDOMHTMLImageElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    SEL useMapSelector = [[self useMap] hasPrefix:@"#"] ? nil : @selector(useMap);
    return [self _URLsFromSelectors:@selector(src), useMapSelector, nil];
}

@end

@implementation DOMHTMLEmbedElement (WebDOMHTMLEmbedElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(src), nil];
}

@end

@implementation DOMHTMLObjectElement (WebDOMHTMLObjectElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    SEL useMapSelector = [[self useMap] hasPrefix:@"#"] ? nil : @selector(useMap);
    return [self _URLsFromSelectors:@selector(data), useMapSelector, nil];
}

@end

@implementation DOMHTMLParamElement (WebDOMHTMLParamElementOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    NSString *name = [self name];
    if ([name _webkit_isCaseInsensitiveEqualToString:@"data"] ||
        [name _webkit_isCaseInsensitiveEqualToString:@"movie"] ||
        [name _webkit_isCaseInsensitiveEqualToString:@"src"]) {
        return [self _URLsFromSelectors:@selector(value), nil];
    }
    return nil;
}

@end

@implementation DOMHTMLTableElement (WebDOMHTMLTableElementOperationsPrivate)

- (NSString *)_web_background
{
    return [self getAttribute:@"background"];
}

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(_web_background), nil];
}

@end

@implementation DOMHTMLTableCellElement (WebDOMHTMLTableCellElementOperationsPrivate)

- (NSString *)_web_background
{
    return [self getAttribute:@"background"];
}

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(_web_background), nil];
}

@end

@implementation DOMHTMLFrameElement (WebDOMHTMLFrameElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

@implementation DOMHTMLIFrameElement (WebDOMHTMLIFrameElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

@implementation DOMHTMLObjectElement (WebDOMHTMLObjectElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end
