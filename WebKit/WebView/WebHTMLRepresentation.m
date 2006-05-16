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

#import <WebKit/WebHTMLRepresentation.h>

#import <WebKit/DOM.h>
#import <WebKit/WebArchive.h>
#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourceInternal.h>
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebImageRendererFactory.h>
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
    WebFrameBridge *bridge;
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

+ (NSArray *)supportedMIMETypes
{
    static NSMutableArray *mimeTypes = nil;
    
    if (!mimeTypes) {
        mimeTypes = [[self supportedNonImageMIMETypes] mutableCopy];
        [mimeTypes addObjectsFromArray:[self supportedImageMIMETypes]];
    }
    
    return mimeTypes;
}

+ (NSArray *)supportedNonImageMIMETypes
{
    return [WebCoreFrameBridge supportedNonImageMIMETypes];
}

+ (NSArray *)supportedImageMIMETypes
{
    [WebImageRendererFactory createSharedFactory];
    return [WebCoreFrameBridge supportedImageMIMETypes];
}

- init
{
    self = [super init];
    if (!self)
        return nil;
    
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

- (WebFrameBridge *)_bridge
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
    [[_private->dataSource webFrame] loadArchive:archive];
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
    return [_private->bridge canProvideDocumentSource];
}

- (BOOL)canSaveAsWebArchive
{
    return [_private->bridge canSaveAsWebArchive];
}

- (NSString *)documentSource
{
    NSData *data = [_private->dataSource data];
    CFStringEncoding encoding = [_private->bridge textEncoding];
    return [WebFrameBridge stringWithData:data textEncoding:encoding];
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
