/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebDataSource.h"

#import "WebArchive.h"
#import "WebArchiver.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDocument.h"
#import "WebDocumentLoaderMac.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoaderClient.h"
#import "WebHTMLRepresentation.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebPDFRepresentation.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourcePrivate.h"
#import "WebUnarchivingState.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/Assertions.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/KURL.h>
#import <WebCore/MimeTypeRegistry.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/SharedBuffer.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/DOMPrivate.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

@interface WebDataSourcePrivate : NSObject
{
    @public
    
    WebDocumentLoaderMac *loader;
   
    id <WebDocumentRepresentation> representation;
    
    WebUnarchivingState *unarchivingState;
    BOOL representationFinishedLoading;
}

@end

@implementation WebDataSourcePrivate 

- (void)dealloc
{
    ASSERT(!loader->isLoading());

    loader->deref();
    
    [representation release];
    [unarchivingState release];

    [super dealloc];
}

@end

@interface WebDataSource (WebFileInternal)
@end

@implementation WebDataSource (WebFileInternal)

- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation
{
    [_private->representation release];
    _private->representation = [representation retain];
    _private->representationFinishedLoading = NO;
}

static inline void addTypesFromClass(NSMutableDictionary *allTypes, Class objCClass, NSArray *supportTypes)
{
    NSEnumerator *enumerator = [supportTypes objectEnumerator];
    ASSERT(enumerator != nil);
    NSString *mime = nil;
    while ((mime = [enumerator nextObject]) != nil) {
        // Don't clobber previously-registered classes.
        if ([allTypes objectForKey:mime] == nil)
            [allTypes setObject:objCClass forKey:mime];
    }
}

+ (Class)_representationClassForMIMEType:(NSString *)MIMEType
{
    Class repClass;
    return [WebView _viewClass:nil andRepresentationClass:&repClass forMIMEType:MIMEType] ? repClass : nil;
}

@end

@implementation WebDataSource (WebPrivate)

- (NSError *)_mainDocumentError
{
    return _private->loader->mainDocumentError();
}

- (void)_addSubframeArchives:(NSArray *)subframeArchives
{
    NSEnumerator *enumerator = [subframeArchives objectEnumerator];
    WebArchive *archive;
    while ((archive = [enumerator nextObject]) != nil)
        [self _addToUnarchiveState:archive];
}

- (NSFileWrapper *)_fileWrapperForURL:(NSURL *)URL
{
    if ([URL isFileURL]) {
        NSString *path = [[URL path] stringByResolvingSymlinksInPath];
        return [[[NSFileWrapper alloc] initWithPath:path] autorelease];
    }
    
    WebResource *resource = [self subresourceForURL:URL];
    if (resource)
        return [resource _fileWrapperRepresentation];
    
    NSCachedURLResponse *cachedResponse = [[self _webView] _cachedResponseForURL:URL];
    if (cachedResponse) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:[cachedResponse data]] autorelease];
        [wrapper setPreferredFilename:[[cachedResponse response] suggestedFilename]];
        return wrapper;
    }
    
    return nil;
}

@end

@implementation WebDataSource (WebInternal)

- (void)_finishedLoading
{
    _private->representationFinishedLoading = YES;
    [[self representation] finishedLoadingWithDataSource:self];
}

- (void)_receivedData:(NSData *)data
{
    // protect self temporarily, as the bridge receivedData call could remove our last ref
    RetainPtr<WebDataSource*> protect(self);
    
    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] frameView] documentView] dataSourceUpdated:self];
}

- (void)_setMainDocumentError:(NSError *)error
{
    if (!_private->representationFinishedLoading) {
        _private->representationFinishedLoading = YES;
        [[self representation] receivedError:error withDataSource:self];
    }
}

- (void)_clearUnarchivingState
{
    [_private->unarchivingState release];
    _private->unarchivingState = nil;
}

- (void)_revertToProvisionalState
{
    [self _setRepresentation:nil];
}

+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static NSMutableDictionary *repTypes = nil;
    static BOOL addedImageTypes = NO;
    
    if (!repTypes) {
        repTypes = [[NSMutableDictionary alloc] init];
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedNonImageMIMETypes]);
        
        // Since this is a "secret default" we don't both registering it.
        BOOL omitPDFSupport = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
        if (!omitPDFSupport)
            addTypesFromClass(repTypes, [WebPDFRepresentation class], [WebPDFRepresentation supportedMIMETypes]);
    }
    
    if (!addedImageTypes && !allowImageTypeOmission) {
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedImageMIMETypes]);
        addedImageTypes = YES;
    }
    
    return repTypes;
}

- (WebResource *)_archivedSubresourceForURL:(NSURL *)URL
{
    return [_private->unarchivingState archivedResourceForURL:URL];
}

- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement
{
    DOMDocumentFragment *fragment = [self _documentFragmentWithArchive:archive];
    if (fragment)
        [[self _bridge] replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:NO matchStyle:NO];
}

- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive
{
    ASSERT(archive);
    WebResource *mainResource = [archive mainResource];
    if (mainResource) {
        NSString *MIMEType = [mainResource MIMEType];
        if ([WebView canShowMIMETypeAsHTML:MIMEType]) {
            NSString *markupString = [[NSString alloc] initWithData:[mainResource data] encoding:NSUTF8StringEncoding];
            // FIXME: seems poor form to do this as a side effect of getting a document fragment
            [self _addToUnarchiveState:archive];
            DOMDocumentFragment *fragment = [[self _bridge] documentFragmentWithMarkupString:markupString baseURLString:[[mainResource URL] _web_originalDataAsString]];
            [markupString release];
            return fragment;
        } else if (MimeTypeRegistry::isSupportedImageMIMEType(MIMEType)) {
            return [self _documentFragmentWithImageResource:mainResource];
            
        }
    }
    return nil;
}

- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource
{
    DOMElement *imageElement = [self _imageElementWithImageResource:resource];
    if (!imageElement)
        return 0;
    DOMDocumentFragment *fragment = [[[self webFrame] DOMDocument] createDocumentFragment];
    [fragment appendChild:imageElement];
    return fragment;
}

- (DOMElement *)_imageElementWithImageResource:(WebResource *)resource
{
    if (!resource)
        return 0;
    
    [self addSubresource:resource];
    
    DOMElement *imageElement = [[[self webFrame] DOMDocument] createElement:@"img"];
    
    // FIXME: calling _web_originalDataAsString on a file URL returns an absolute path. Workaround this.
    NSURL *URL = [resource URL];
    [imageElement setAttribute:@"src" value:[URL isFileURL] ? [URL absoluteString] : [URL _web_originalDataAsString]];
    
    return imageElement;
}

// May return nil if not initialized with a URL.
- (NSURL *)_URL
{
    KURL URL = _private->loader->URL();
    return URL.isEmpty() ? nil : URL.getNSURL();
}

- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName
{
    return [_private->unarchivingState popSubframeArchiveWithFrameName:frameName];
}

- (WebFrameBridge *)_bridge
{
    ASSERT(_private->loader->isCommitted());
    return [[self webFrame] _bridge];
}

- (WebView *)_webView
{
    return [[self webFrame] webView];
}

- (BOOL)_isDocumentHTML
{
    NSString *MIMEType = [[self response] MIMEType];
    return [WebView canShowMIMETypeAsHTML:MIMEType];
}

-(void)_makeRepresentation
{
    Class repClass = [[self class] _representationClassForMIMEType:[[self response] MIMEType]];
    
    // Check if the data source was already bound?
    if (![[self representation] isKindOfClass:repClass]) {
        id newRep = repClass != nil ? [[repClass alloc] init] : nil;
        [self _setRepresentation:(id <WebDocumentRepresentation>)newRep];
        [newRep release];
    }
    
    [_private->representation setDataSource:self];
}

- (void)_addToUnarchiveState:(WebArchive *)archive
{
    if (!_private->unarchivingState)
        _private->unarchivingState = [[WebUnarchivingState alloc] init];
    [_private->unarchivingState addArchive:archive];
}

- (DocumentLoader*)_documentLoader
{
    return _private->loader;
}

- (id)_initWithDocumentLoader:(WebDocumentLoaderMac *)loader
{
    self = [super init];
    if (!self)
        return nil;
    
    _private = [[WebDataSourcePrivate alloc] init];
    
    _private->loader = loader;
    loader->ref();
        
    LOG(Loading, "creating datasource for %@", _private->loader->request().url().getNSURL());
    
    ++WebDataSourceCount;
    
    return self;    
}

@end

@implementation WebDataSource

- (id)initWithRequest:(NSURLRequest *)request
{
    return [self _initWithDocumentLoader:new WebDocumentLoaderMac(request, SubstituteData())];
}

- (void)dealloc
{
    --WebDataSourceCount;
    
    [_private release];
    
    [super dealloc];
}

- (void)finalize
{
    --WebDataSourceCount;

    [super finalize];
}

- (NSData *)data
{
    RefPtr<SharedBuffer> mainResourceData = _private->loader->mainResourceData();
    if (!mainResourceData)
        return nil;
    return [mainResourceData->createNSData() autorelease];
}

- (id <WebDocumentRepresentation>)representation
{
    return _private->representation;
}

- (WebFrame *)webFrame
{
    FrameLoader* frameLoader = _private->loader->frameLoader();
    if (!frameLoader)
        return nil;
    return static_cast<WebFrameLoaderClient*>(frameLoader->client())->webFrame();
}

- (NSURLRequest *)initialRequest
{
    return _private->loader->initialRequest().nsURLRequest();
}

- (NSMutableURLRequest *)request
{
    // FIXME: XXX
    return (NSMutableURLRequest*)_private->loader->request().nsURLRequest();
}

- (NSURLResponse *)response
{
    return _private->loader->response().nsURLResponse();
}

- (NSString *)textEncodingName
{
    NSString *textEncodingName = _private->loader->overrideEncoding();
    if (!textEncodingName)
        textEncodingName = [[self response] textEncodingName];
    return textEncodingName;
}

- (BOOL)isLoading
{
    return _private->loader->isLoadingInAPISense();
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return [[self representation] title];
}

- (NSURL *)unreachableURL
{
    KURL URL = _private->loader->unreachableURL();
    return URL.isEmpty() ? nil : URL.getNSURL();
}

- (WebArchive *)webArchive
{
    // it makes no sense to grab a WebArchive from an uncommitted document.
    if (!_private->loader->isCommitted())
        return nil;
    return [WebArchiver archiveFrame:[self webFrame]];
}

- (WebResource *)mainResource
{
    NSURLResponse *response = [self response];
    return [[[WebResource alloc] initWithData:[self data]
                                          URL:[response URL] 
                                     MIMEType:[response MIMEType]
                             textEncodingName:[response textEncodingName]
                                    frameName:[[self webFrame] name]] autorelease];
}

- (NSArray *)subresources
{
    if (!_private->loader->isCommitted())
        return [NSMutableArray array];

    NSArray *datas;
    NSArray *responses;
    [[self _bridge] getAllResourceDatas:&datas andResponses:&responses];
    ASSERT([datas count] == [responses count]);

    NSMutableArray *subresources = [[NSMutableArray alloc] initWithCapacity:[datas count]];
    for (unsigned i = 0; i < [datas count]; ++i) {
        NSURLResponse *response = [responses objectAtIndex:i];
        [subresources addObject:[[[WebResource alloc] _initWithData:[datas objectAtIndex:i] URL:[response URL] response:response] autorelease]];
    }

    return [subresources autorelease];
}

- (WebResource *)subresourceForURL:(NSURL *)URL
{
    if (!_private->loader->isCommitted())
        return nil;

    NSData *data;
    NSURLResponse *response;
    if (![[self _bridge] getData:&data andResponse:&response forURL:[URL _web_originalDataAsString]])
        return [self _archivedSubresourceForURL:URL];

    return [[[WebResource alloc] _initWithData:data URL:URL response:response] autorelease];
}

- (void)addSubresource:(WebResource *)subresource
{
    if (subresource) {
        if (!_private->unarchivingState)
            _private->unarchivingState = [[WebUnarchivingState alloc] init];
        [_private->unarchivingState addResource:subresource];
    }
}

@end
