/*
    WebDOMOperations.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDOMOperations.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>

@interface DOMNode (WebDOMNodeOperationsPrivate)
- (WebBridge *)_bridge;
@end

@interface DOMRange (WebDOMRangeOperationsPrivate)
- (WebBridge *)_bridge;
@end

@implementation DOMNode (WebDOMNodeOperations)

- (WebBridge *)_bridge
{
    return (WebBridge *)[WebBridge bridgeForDOMDocument:[self ownerDocument]];
}

- (WebArchive *)webArchive
{
    WebBridge *bridge = [self _bridge];
    NSArray *subresourceURLStrings;
    NSString *markupString = [bridge markupStringFromNode:self subresourceURLStrings:&subresourceURLStrings];
    return [[[bridge webFrame] dataSource] _archiveWithMarkupString:markupString subresourceURLStrings:subresourceURLStrings];
}

- (NSString *)markupString
{
    return [[self _bridge] markupStringFromNode:self subresourceURLStrings:nil];
}

@end

@implementation DOMDocument (WebDOMDocumentOperations)

- (NSURL *)URLWithRelativeString:(NSString *)string
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
    NSArray *subresourceURLStrings;
    NSString *markupString = [bridge markupStringFromRange:self subresourceURLStrings:&subresourceURLStrings];
    return [[[bridge webFrame] dataSource] _archiveWithMarkupString:markupString subresourceURLStrings:subresourceURLStrings];
}

- (NSString *)markupString
{		
    return [[self _bridge] markupStringFromRange:self subresourceURLStrings:nil];
}

@end
