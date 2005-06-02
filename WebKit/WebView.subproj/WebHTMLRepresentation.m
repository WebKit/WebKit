/*	
        WebHTMLRepresentation.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLRepresentation.h>

#import <WebKit/DOM.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebView.h>

#import <Foundation/NSURLResponse.h>

@interface WebHTMLRepresentationPrivate : NSObject
{
@public
    WebDataSource *dataSource;
    WebBridge *bridge;
    NSData *parsedArchiveData;
}
@end

@implementation WebHTMLRepresentationPrivate

- (void)dealloc
{
    [parsedArchiveData release];
    [super dealloc];
}

@end

@implementation WebHTMLRepresentation

- init
{
    [super init];
    
    _private = [[WebHTMLRepresentationPrivate alloc] init];
    
    ++WebHTMLRepresentationCount;
    
    return self;
}

- (void)dealloc
{
    --WebHTMLRepresentationCount;
    
    [_private release];

    [super dealloc];
}

- (void)finalize
{
    --WebHTMLRepresentationCount;

    [super finalize];
}

- (WebBridge *)_bridge
{
    return _private->bridge;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    _private->dataSource = dataSource;
    _private->bridge = [[dataSource webFrame] _bridge];
}

- (BOOL)_isDisplayingWebArchive
{
    return [[[_private->dataSource response] MIMEType] _webkit_isCaseInsensitiveEqualToString:@"application/x-webarchive"];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    if ([dataSource webFrame] && ![self _isDisplayingWebArchive]) {
        [_private->bridge receivedData:data textEncodingName:[[_private->dataSource response] textEncodingName]];
    }
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
}

- (void)loadArchive
{
    WebArchive *archive = [[WebArchive alloc] initWithData:[_private->dataSource data]];
    WebResource *mainResource = [archive mainResource];
    NSArray *subresources = [archive subresources];
    NSArray *subframeArchives = [archive subframeArchives];
    [archive release];
    if (!mainResource) {
        return;
    }
    
    NSData *data = [mainResource data];
    [data retain];
    [_private->parsedArchiveData release];
    _private->parsedArchiveData = data;
    
    [_private->dataSource _addSubresources:subresources];
    [_private->dataSource _addSubframeArchives:subframeArchives];
    [_private->bridge closeURL];
    [_private->bridge openURL:[mainResource URL]
                       reload:NO 
                  contentType:[mainResource MIMEType]
                      refresh:NO
                 lastModified:nil
                    pageCache:nil];
    [_private->bridge receivedData:data textEncodingName:[mainResource textEncodingName]];
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];
    if (frame) {
        if ([self _isDisplayingWebArchive]) {
            [self loadArchive];
        } else {
            // Telling the bridge we received some data and passing nil as the data is our
            // way to get work done that is normally done when the first bit of data is
            // received, even for the case of a document with no data (like about:blank).
            [_private->bridge receivedData:nil textEncodingName:[[_private->dataSource response] textEncodingName]];
        }
        
        WebView *webView = [frame webView];
        if ([webView isEditable]) {
            [_private->bridge applyEditingStyleToBodyElement];
        }
    }
}

- (BOOL)canProvideDocumentSource
{
    return YES;
}

- (NSString *)documentSource
{
    if ([self _isDisplayingWebArchive]) {
        return [[[NSString alloc] initWithData:_private->parsedArchiveData encoding:NSUTF8StringEncoding] autorelease];
    } else {
        return [WebBridge stringWithData:[_private->dataSource data] textEncoding:[_private->bridge textEncoding]];
    }
}

- (NSString *)title
{
    return [_private->dataSource _title];
}

- (DOMDocument *)DOMDocument
{
    return [_private->bridge DOMDocument];
}

- (NSAttributedString *)attributedText
{
    // FIXME:  Implement
    return nil;
}

- (NSAttributedString *)attributedStringFrom: (DOMNode *)startNode startOffset: (int)startOffset to: (DOMNode *)endNode endOffset: (int)endOffset
{
    return [_private->bridge attributedStringFrom: startNode startOffset: startOffset to: endNode endOffset: endOffset];
}

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form
{
    return [_private->bridge elementWithName:name inForm:form];
}

- (DOMElement *)elementForView:(NSView *)view
{
    return [_private->bridge elementForView:view];
}

- (BOOL)elementDoesAutoComplete:(DOMElement *)element
{
    return [_private->bridge elementDoesAutoComplete:element];
}

- (BOOL)elementIsPassword:(DOMElement *)element
{
    return [_private->bridge elementIsPassword:element];
}

- (DOMElement *)formForElement:(DOMElement *)element
{
    return [_private->bridge formForElement:element];
}

- (DOMElement *)currentForm
{
    return [_private->bridge currentForm];
}

- (NSArray *)controlsInForm:(DOMElement *)form
{
    return [_private->bridge controlsInForm:form];
}

- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element
{
    return [_private->bridge searchForLabels:labels beforeElement:element];
}

- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element
{
    return [_private->bridge matchLabels:labels againstElement:element];
}

@end
