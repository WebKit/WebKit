/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2012 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#import "WebArchiveInternal.h"
#import "WebDataSourceInternal.h"
#import "WebDocument.h"
#import "WebDocumentLoaderMac.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentation.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebPDFRepresentation.h"
#import "WebResourceInternal.h"
#import "WebResourceLoadDelegate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebCoreURLResponse.h>
#import <WebKitLegacy/DOMHTML.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/NakedPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>

#if PLATFORM(IOS_FAMILY)
#import "WebPDFViewIOS.h"
#endif

#if USE(QUICK_LOOK)
#import <WebCore/LegacyPreviewLoaderClient.h>
#import <WebCore/QuickLook.h>
#endif

class WebDataSourcePrivate
{
public:
    WebDataSourcePrivate(Ref<WebDocumentLoaderMac>&& loader)
        : loader(WTFMove(loader))
        , representationFinishedLoading(NO)
        , includedInWebKitStatistics(NO)
#if PLATFORM(IOS_FAMILY)
        , _dataSourceDelegate(nil)
#endif
    {
    }
    ~WebDataSourcePrivate()
    {
        // We might run in to infinite recursion if we're stopping loading as the result of detaching from the frame.
        // Therefore, DocumentLoader::detachFromFrame() did some smart things to stop the recursion.
        // As a result of breaking the resursion, DocumentLoader::m_subresourceLoader
        // and DocumentLoader::m_plugInStreamLoaders might not be empty at this time.
        // See <rdar://problem/9673866> for more details.
        ASSERT(!loader->isLoading() || loader->isStopping());
        loader->detachDataSource();
    }

    Ref<WebDocumentLoaderMac> loader;
    RetainPtr<id<WebDocumentRepresentation> > representation;
    BOOL representationFinishedLoading;
    BOOL includedInWebKitStatistics;
#if PLATFORM(IOS_FAMILY)
    NSObject<WebDataSourcePrivateDelegate> *_dataSourceDelegate;
#endif
#if USE(QUICK_LOOK)
    RetainPtr<NSDictionary> _quickLookContent;
    RefPtr<WebCore::LegacyPreviewLoaderClient> _quickLookPreviewLoaderClient;
#endif
};

static inline WebDataSourcePrivate* toPrivate(void* privateAttribute)
{
    return reinterpret_cast<WebDataSourcePrivate*>(privateAttribute);
}

@interface WebDataSource (WebFileInternal)
@end

@implementation WebDataSource (WebFileInternal)

- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation
{
    toPrivate(_private)->representation = representation;
    toPrivate(_private)->representationFinishedLoading = NO;
}

void addTypesFromClass(NSMutableDictionary *allTypes, Class objCClass, NSArray *supportTypes)
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

+ (Class)_representationClassForMIMEType:(NSString *)MIMEType allowingPlugins:(BOOL)allowPlugins
{
    Class repClass;
    return [WebView _viewClass:nil andRepresentationClass:&repClass forMIMEType:MIMEType allowingPlugins:allowPlugins] ? repClass : nil;
}
@end

@implementation WebDataSource (WebPrivate)

+ (void)initialize
{
    if (self == [WebDataSource class]) {
#if !PLATFORM(IOS_FAMILY)
        JSC::initializeThreading();
        WTF::initializeMainThread();
#endif
    }
}

- (NSError *)_mainDocumentError
{
    return toPrivate(_private)->loader->mainDocumentError();
}

- (void)_addSubframeArchives:(NSArray *)subframeArchives
{
    // FIXME: This SPI is poor, poor design. Can we come up with another solution for those who need it?
    for (WebArchive *archive in subframeArchives)
        toPrivate(_private)->loader->addAllArchiveResources(*[archive _coreLegacyWebArchive]);
}

#if !PLATFORM(IOS_FAMILY)

- (NSFileWrapper *)_fileWrapperForURL:(NSURL *)URL
{
    if ([URL isFileURL])
        return [[[NSFileWrapper alloc] initWithURL:[URL URLByResolvingSymlinksInPath] options:0 error:nullptr] autorelease];

    if (auto resource = [self subresourceForURL:URL])
        return [resource _fileWrapperRepresentation];

    if (auto cachedResponse = [[self _webView] _cachedResponseForURL:URL]) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:[cachedResponse data]] autorelease];
        [wrapper setPreferredFilename:[[cachedResponse response] suggestedFilename]];
        return wrapper;
    }
    
    return nil;
}

#endif

- (NSString *)_responseMIMEType
{
    return [[self response] MIMEType];
}

- (void)_setDeferMainResourceDataLoad:(BOOL)flag
{
    toPrivate(_private)->loader->setDeferMainResourceDataLoad(flag);
}

#if PLATFORM(IOS_FAMILY)
- (void)_setOverrideTextEncodingName:(NSString *)encoding
{
    toPrivate(_private)->loader->setOverrideEncoding([encoding UTF8String]);
}
#endif

- (void)_setAllowToBeMemoryMapped
{
}

- (void)setDataSourceDelegate:(NSObject<WebDataSourcePrivateDelegate> *)delegate
{
}

- (NSObject<WebDataSourcePrivateDelegate> *)dataSourceDelegate
{
    return nullptr;
}

#if PLATFORM(IOS_FAMILY)
- (NSDictionary *)_quickLookContent
{
#if USE(QUICK_LOOK)
    return toPrivate(_private)->_quickLookContent.get();
#else
    return nil;
#endif
}
#endif

@end

@implementation WebDataSource (WebInternal)

- (void)_finishedLoading
{
    toPrivate(_private)->representationFinishedLoading = YES;
    [[self representation] finishedLoadingWithDataSource:self];
}

- (void)_receivedData:(NSData *)data
{
    // protect self temporarily, as the bridge receivedData call could remove our last ref
    RetainPtr<WebDataSource> protect(self);
    
    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] frameView] documentView] dataSourceUpdated:self];
}

- (void)_setMainDocumentError:(NSError *)error
{
    if (!toPrivate(_private)->representationFinishedLoading) {
        toPrivate(_private)->representationFinishedLoading = YES;
        [[self representation] receivedError:error withDataSource:self];
    }
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
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedMediaMIMETypes]);
        
        // Since this is a "secret default" we don't both registering it.
        BOOL omitPDFSupport = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
        if (!omitPDFSupport)
#if PLATFORM(IOS_FAMILY)
#define WebPDFRepresentation ([WebView _getPDFRepresentationClass])
#endif
            addTypesFromClass(repTypes, [WebPDFRepresentation class], [WebPDFRepresentation supportedMIMETypes]);
#if PLATFORM(IOS_FAMILY)
#undef WebPDFRepresentation
#endif
    }
    
    if (!addedImageTypes && !allowImageTypeOmission) {
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedImageMIMETypes]);
        addedImageTypes = YES;
    }
    
    return repTypes;
}

- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement
{
    DOMDocumentFragment *fragment = [self _documentFragmentWithArchive:archive];
    if (fragment)
        [[self webFrame] _replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:NO matchStyle:NO];
}

// FIXME: There are few reasons why this method and many of its related methods can't be pushed entirely into WebCore in the future.
- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive
{
    ASSERT(archive);
    WebResource *mainResource = [archive mainResource];
    if (mainResource) {
        NSString *MIMEType = [mainResource MIMEType];
        if ([WebView canShowMIMETypeAsHTML:MIMEType]) {
            NSString *markupString = [[NSString alloc] initWithData:[mainResource data] encoding:NSUTF8StringEncoding];

            // FIXME: seems poor form to do this as a side effect of getting a document fragment
            toPrivate(_private)->loader->addAllArchiveResources(*[archive _coreLegacyWebArchive]);

            DOMDocumentFragment *fragment = [[self webFrame] _documentFragmentWithMarkupString:markupString baseURLString:[[mainResource URL] _web_originalDataAsString]];
            [markupString release];
            return fragment;
        } else if (WebCore::MIMETypeRegistry::isSupportedImageMIMEType(MIMEType)) {
            return [self _documentFragmentWithImageResource:mainResource];
            
        }
    }
    return nil;
}

- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource
{
    DOMElement *imageElement = [self _imageElementWithImageResource:resource];
    if (!imageElement)
        return nil;
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
    const URL& url = toPrivate(_private)->loader->url();
    if (url.isEmpty())
        return nil;
    return url;
}

- (WebView *)_webView
{
    return [[self webFrame] webView];
}

- (BOOL)_isDocumentHTML
{
    NSString *MIMEType = [self _responseMIMEType];
    return [WebView canShowMIMETypeAsHTML:MIMEType];
}

- (void)_makeRepresentation
{
    Class repClass = [[self class] _representationClassForMIMEType:[self _responseMIMEType] allowingPlugins:[[[self _webView] preferences] arePlugInsEnabled]];

#if PLATFORM(IOS_FAMILY)
    if ([repClass respondsToSelector:@selector(_representationClassForWebFrame:)])
        repClass = [repClass performSelector:@selector(_representationClassForWebFrame:) withObject:[self webFrame]];
#endif

    // Check if the data source was already bound?
    if (![[self representation] isKindOfClass:repClass]) {
        id newRep = repClass != nil ? [(NSObject *)[repClass alloc] init] : nil;
        [self _setRepresentation:(id <WebDocumentRepresentation>)newRep];
        [newRep release];
    }

    id<WebDocumentRepresentation> representation = toPrivate(_private)->representation.get();
    [representation setDataSource:self];
#if PLATFORM(IOS_FAMILY)
    toPrivate(_private)->loader->setResponseMIMEType([self _responseMIMEType]);
#endif
}

- (NakedPtr<WebCore::DocumentLoader>)_documentLoader
{
    return toPrivate(_private)->loader.ptr();
}

- (id)_initWithDocumentLoader:(Ref<WebDocumentLoaderMac>&&)loader
{
    self = [super init];
    if (!self)
        return nil;

    _private = static_cast<void*>(new WebDataSourcePrivate(WTFMove(loader)));
        
    LOG(Loading, "creating datasource for %@", static_cast<NSURL *>(toPrivate(_private)->loader->request().url()));

    if ((toPrivate(_private)->includedInWebKitStatistics = [[self webFrame] _isIncludedInWebKitStatistics]))
        ++WebDataSourceCount;

    return self;
}

#if USE(QUICK_LOOK)
- (WebCore::LegacyPreviewLoaderClient*)_quickLookPreviewLoaderClient
{
    return toPrivate(_private)->_quickLookPreviewLoaderClient.get();
}

- (void)_setQuickLookContent:(NSDictionary *)quickLookContent
{
    toPrivate(_private)->_quickLookContent = adoptNS([quickLookContent copy]);
}

- (void)_setQuickLookPreviewLoaderClient:(WebCore::LegacyPreviewLoaderClient*)quickLookPreviewLoaderClient
{
    toPrivate(_private)->_quickLookPreviewLoaderClient = quickLookPreviewLoaderClient;
}
#endif

@end

@implementation WebDataSource

- (instancetype)initWithRequest:(NSURLRequest *)request
{
    return [self _initWithDocumentLoader:WebDocumentLoaderMac::create(request, WebCore::SubstituteData())];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebDataSource class], self))
        return;

    if (toPrivate(_private) && toPrivate(_private)->includedInWebKitStatistics)
        --WebDataSourceCount;

#if USE(QUICK_LOOK)
    // Added in -[WebCoreResourceHandleAsDelegate connection:didReceiveResponse:].
    if (NSURL *url = [[self response] URL])
        WebCore::removeQLPreviewConverterForURL(url);
#endif

    delete toPrivate(_private);

    [super dealloc];
}

- (NSData *)data
{
    RefPtr<WebCore::SharedBuffer> mainResourceData = toPrivate(_private)->loader->mainResourceData();
    if (!mainResourceData)
        return nil;
    return mainResourceData->createNSData().autorelease();
}

- (id <WebDocumentRepresentation>)representation
{
    return toPrivate(_private)->representation.get();
}

- (WebFrame *)webFrame
{
    if (auto* frame = toPrivate(_private)->loader->frame())
        return kit(frame);

    return nil;
}

- (NSURLRequest *)initialRequest
{
    return toPrivate(_private)->loader->originalRequest().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
}

- (NSMutableURLRequest *)request
{
    auto* frameLoader = toPrivate(_private)->loader->frameLoader();
    if (!frameLoader || !frameLoader->frameHasLoaded())
        return nil;

    // FIXME: this cast is dubious
    return (NSMutableURLRequest *)toPrivate(_private)->loader->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
}

- (NSURLResponse *)response
{
    return toPrivate(_private)->loader->response().nsURLResponse();
}

- (NSString *)textEncodingName
{
    NSString *textEncodingName = toPrivate(_private)->loader->overrideEncoding();
    if (!textEncodingName)
        textEncodingName = [[self response] textEncodingName];
    return textEncodingName;
}

- (BOOL)isLoading
{
    return toPrivate(_private)->loader->isLoadingInAPISense();
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return [[self representation] title];
}

- (NSURL *)unreachableURL
{
    const URL& unreachableURL = toPrivate(_private)->loader->unreachableURL();
    if (unreachableURL.isEmpty())
        return nil;
    return unreachableURL;
}

- (WebArchive *)webArchive
{
    // it makes no sense to grab a WebArchive from an uncommitted document.
    if (!toPrivate(_private)->loader->isCommitted())
        return nil;
        
    return [[[WebArchive alloc] _initWithCoreLegacyWebArchive:WebCore::LegacyWebArchive::create(*core([self webFrame]))] autorelease];
}

- (WebResource *)mainResource
{
    auto coreResource = toPrivate(_private)->loader->mainResource();
    if (!coreResource)
        return nil;
    return [[[WebResource alloc] _initWithCoreResource:coreResource.releaseNonNull()] autorelease];
}

- (NSArray *)subresources
{
    return createNSArray(toPrivate(_private)->loader->subresources(), [] (auto& resource) {
        return adoptNS([[WebResource alloc] _initWithCoreResource:resource.copyRef()]);
    }).autorelease();
}

- (WebResource *)subresourceForURL:(NSURL *)URL
{
    auto subresource = toPrivate(_private)->loader->subresource(URL);
    return subresource ? [[[WebResource alloc] _initWithCoreResource:subresource.releaseNonNull()] autorelease] : nil;
}

- (void)addSubresource:(WebResource *)subresource
{    
    toPrivate(_private)->loader->addArchiveResource([subresource _coreResource].get());
}

@end
