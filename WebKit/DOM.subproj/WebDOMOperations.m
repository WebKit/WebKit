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

#import <Foundation/NSString_NSURLExtras.h>

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
    return [[self _bridge] URLWithRelativeString:string];
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

@implementation DOMHTMLBodyElement (WebDOMHTMLBodyElementOperations)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(background), nil];
}

@end

@implementation DOMHTMLInputElement (WebDOMHTMLInputElementOperations)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(src), nil];
}

@end

@implementation DOMHTMLLinkElement (WebDOMHTMLLinkElementOperations)

- (NSArray *)_subresourceURLs
{
    NSString *rel = [self rel];
    if ([rel _web_isCaseInsensitiveEqualToString:@"stylesheet"] || [rel _web_isCaseInsensitiveEqualToString:@"icon"]) {
        return [self _URLsFromSelectors:@selector(href), nil];
    }
    return nil;
}

@end

@implementation DOMHTMLScriptElement (WebDOMHTMLScriptElementOperations)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(src), nil];
}

@end

@implementation DOMHTMLImageElement (WebDOMHTMLImageElementOperations)

- (NSArray *)_subresourceURLs
{
    SEL useMapSelector = [[self useMap] hasPrefix:@"#"] ? nil : @selector(useMap);
    return [self _URLsFromSelectors:@selector(src), useMapSelector, nil];
}

@end

@implementation DOMHTMLEmbedElement (WebDOMHTMLEmbedElementOperations)

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(src), nil];
}

@end

@implementation DOMHTMLObjectElement (WebDOMHTMLObjectElementOperations)

- (NSArray *)_subresourceURLs
{
    SEL useMapSelector = [[self useMap] hasPrefix:@"#"] ? nil : @selector(useMap);
    return [self _URLsFromSelectors:@selector(data), useMapSelector, nil];
}

@end

@implementation DOMHTMLParamElement (WebDOMHTMLParamElementOperations)

- (NSArray *)_subresourceURLs
{
    NSString *name = [self name];
    if ([name _web_isCaseInsensitiveEqualToString:@"data"] ||
        [name _web_isCaseInsensitiveEqualToString:@"movie"] ||
        [name _web_isCaseInsensitiveEqualToString:@"src"]) {
        return [self _URLsFromSelectors:@selector(value), nil];
    }
    return nil;
}

@end

@implementation DOMHTMLTableElement (WebDOMHTMLTableElementOperations)

- (NSString *)_web_background
{
    return [self getAttribute:@"background"];
}

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(_web_background), nil];
}

@end

@implementation DOMHTMLTableCellElement (WebDOMHTMLTableCellElementOperations)

- (NSString *)_web_background
{
    return [self getAttribute:@"background"];
}

- (NSArray *)_subresourceURLs
{
    return [self _URLsFromSelectors:@selector(_web_background), nil];
}

@end
