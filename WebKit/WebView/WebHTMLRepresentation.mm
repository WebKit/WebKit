/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#import "WebHTMLRepresentation.h"

#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebArchive.h"
#import "WebBasePluginPackage.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentPrivate.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSAttributedStringExtras.h"
#import "WebNSObjectExtras.h"
#import "WebResourcePrivate.h"
#import "WebView.h"
#import <Foundation/NSURLResponse.h>
#import <JavaScriptCore/Assertions.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameMac.h>
#import <WebCore/MimeTypeRegistry.h>
#import <WebCore/Range.h>

using namespace WebCore;

@interface WebHTMLRepresentationPrivate : NSObject
{
@public
    WebDataSource *dataSource;
    WebFrameBridge *bridge;
    NSData *parsedArchiveData;
    
    BOOL hasSentResponseToPlugin;
    id <WebPluginManualLoader> manualLoader;
    NSView *pluginView;
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

static NSArray *stringArray(const HashSet<String>& set)
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:set.size()];
    HashSet<String>::const_iterator end = set.end();
    for (HashSet<String>::const_iterator it = set.begin(); it != end; ++it)
        [array addObject:(NSString *)(*it)];
    return array;
}

static NSArray *concatenateArrays(NSArray *first, NSArray *second)
{
    NSMutableArray *result = [[first mutableCopy] autorelease];
    [result addObjectsFromArray:second];
    return result;
}

+ (NSArray *)supportedMIMETypes
{
    static RetainPtr<NSArray> staticSupportedMIMETypes =
        concatenateArrays([self supportedNonImageMIMETypes], [self supportedImageMIMETypes]);
    return staticSupportedMIMETypes.get();
}

+ (NSArray *)supportedNonImageMIMETypes
{
    static RetainPtr<NSArray> staticSupportedNonImageMIMETypes =
        stringArray(MimeTypeRegistry::getSupportedNonImageMIMETypes());
    return staticSupportedNonImageMIMETypes.get();
}

+ (NSArray *)supportedImageMIMETypes
{
    static RetainPtr<NSArray> staticSupportedImageMIMETypes =
        stringArray(MimeTypeRegistry::getSupportedImageMIMETypes());
    return staticSupportedImageMIMETypes.get();
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

- (void)_redirectDataToManualLoader:(id<WebPluginManualLoader>)manualLoader forPluginView:(NSView *)pluginView;
{
    _private->manualLoader = manualLoader;
    _private->pluginView = pluginView;
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
        if (!_private->pluginView)
            [_private->bridge receivedData:data textEncodingName:[[_private->dataSource response] textEncodingName]];

        if (_private->pluginView) {
            if (!_private->hasSentResponseToPlugin) {
                [_private->manualLoader pluginView:_private->pluginView receivedResponse:[dataSource response]];
                _private->hasSentResponseToPlugin = YES;
            }
            
            [_private->manualLoader pluginView:_private->pluginView receivedData:data];
        }
    }
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
    if (_private->pluginView) {
        [_private->manualLoader pluginView:_private->pluginView receivedError:error];
    }
}

- (void)_loadDataSourceAsWebArchive
{
    WebArchive *archive = [[WebArchive alloc] initWithData:[_private->dataSource data]];
    WebResource *mainResource = [archive mainResource];
    if (!mainResource) {
        [archive release];
        return;
    }
    
    NSData *data = [mainResource data];
    [data retain];
    [_private->parsedArchiveData release];
    _private->parsedArchiveData = data;
    
    [_private->dataSource _addToUnarchiveState:archive];
    [archive release];
    
    WebFrame *webFrame = [_private->dataSource webFrame];
    
    if (!webFrame)
        return;
    
    core(webFrame)->loader()->continueLoadWithData(SharedBuffer::wrapNSData(data).get(), [mainResource MIMEType], [mainResource textEncodingName], [mainResource URL]);
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];

    if (_private->pluginView) {
        [_private->manualLoader pluginViewFinishedLoading:_private->pluginView];
        return;
    }

    if (frame) {
        if ([self _isDisplayingWebArchive])
            [self _loadDataSourceAsWebArchive];
        else
            // Telling the bridge we received some data and passing nil as the data is our
            // way to get work done that is normally done when the first bit of data is
            // received, even for the case of a document with no data (like about:blank).
            [_private->bridge receivedData:nil textEncodingName:[[_private->dataSource response] textEncodingName]];
        
        WebView *webView = [frame webView];
        if ([webView isEditable])
            core(frame)->applyEditingStyleToBodyElement();
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
    if ([self _isDisplayingWebArchive])
        return [[[NSString alloc] initWithData:_private->parsedArchiveData encoding:NSUTF8StringEncoding] autorelease]; 

    return [_private->bridge stringWithData:[_private->dataSource data]];
}

- (NSString *)title
{
    return [_private->dataSource _documentLoader]->title();
}

- (DOMDocument *)DOMDocument
{
    return [[_private->bridge webFrame] DOMDocument];
}

- (NSAttributedString *)attributedText
{
    // FIXME: Implement
    return nil;
}

- (NSAttributedString *)attributedStringFrom:(DOMNode *)startNode startOffset:(int)startOffset to:(DOMNode *)endNode endOffset:(int)endOffset
{
    Range range([startNode _node]->document(), [startNode _node], startOffset, [endNode _node], endOffset);
    return [NSAttributedString _web_attributedStringFromRange:&range];
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
