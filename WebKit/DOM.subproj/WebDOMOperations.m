/*
    WebDOMOperations.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDOMOperationsPrivate.h>

#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitNSStringExtras.h>


@implementation DOMNode (WebDOMNodeOperations)

- (WebBridge *)_bridge
{
    return (WebBridge *)[WebBridge bridgeForDOMDocument:[self ownerDocument]];
}

- (WebArchive *)webArchive
{
    WebBridge *bridge = [self _bridge];
    NSArray *nodes;
    NSString *markupString = [bridge markupStringFromNode:self nodes:&nodes];
    return [[[bridge webFrame] dataSource] _archiveWithMarkupString:markupString nodes:nodes];
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

- (WebBridge *)_bridge
{
    return [[self startContainer] _bridge];
}

- (WebArchive *)webArchive
{
    WebBridge *bridge = [self _bridge];
    NSArray *nodes;
    NSString *markupString = [bridge markupStringFromRange:self nodes:&nodes];
    return [[[bridge webFrame] dataSource] _archiveWithMarkupString:markupString nodes:nodes];
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
