/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "QuickLook.h"

#if USE(QUICK_LOOK)

#import "DocumentLoader.h"
#import "FileSystemIOS.h"
#import "Logging.h"
#import "ResourceError.h"
#import "ResourceHandle.h"
#import "ResourceLoader.h"
#import "RuntimeApplicationChecksIOS.h"
#import "SoftLinking.h"
#import "SynchronousResourceHandleCFURLConnectionDelegate.h"
#import "WebCoreURLResponseIOS.h"
#import <Foundation/Foundation.h>
#import <Foundation/NSFileManager_NSURLExtras.h>
#import <QuickLook/QLPreviewConverter.h>
#import <QuickLook/QuickLookPrivate.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

#if USE(CFNETWORK)
#import <CFNetwork/CFURLConnection.h>

@interface NSURLResponse (QuickLookDetails)
+(NSURLResponse *)_responseWithCFURLResponse:(CFURLResponseRef)response;
-(CFURLResponseRef)_CFURLResponse;
@end
#endif

SOFT_LINK_FRAMEWORK_OPTIONAL(QuickLook)
SOFT_LINK_CLASS(QuickLook, QLPreviewConverter)
SOFT_LINK_MAY_FAIL(QuickLook, QLPreviewGetSupportedMIMETypes, NSSet *, (), ())
SOFT_LINK_MAY_FAIL(QuickLook, QLTypeCopyBestMimeTypeForFileNameAndMimeType, NSString *, (NSString *fileName, NSString *mimeType), (fileName, mimeType))
SOFT_LINK_MAY_FAIL(QuickLook, QLTypeCopyBestMimeTypeForURLAndMimeType, NSString *, (NSURL *url, NSString *mimeType), (url, mimeType))
SOFT_LINK_MAY_FAIL(QuickLook, QLTypeCopyUTIForURLAndMimeType, NSString *, (NSURL *url, NSString *mimeType), (url, mimeType))
SOFT_LINK_CONSTANT_MAY_FAIL(QuickLook, QLPreviewScheme, NSString *)

namespace WebCore {
    NSString *QLTypeCopyUTIForURLAndMimeType(NSURL *url, NSString *mimeType);
}

using namespace WebCore;

Class WebCore::QLPreviewConverterClass()
{
#define QLPreviewConverter getQLPreviewConverterClass()
    return QLPreviewConverter;
#undef QLPreviewConverter
}

NSString *WebCore::QLTypeCopyBestMimeTypeForFileNameAndMimeType(NSString *fileName, NSString *mimeType)
{
    if (!canLoadQLTypeCopyBestMimeTypeForFileNameAndMimeType())
        return nil;

    return ::QLTypeCopyBestMimeTypeForFileNameAndMimeType(fileName, mimeType);
}

NSString *WebCore::QLTypeCopyBestMimeTypeForURLAndMimeType(NSURL *url, NSString *mimeType)
{
    if (!canLoadQLTypeCopyBestMimeTypeForURLAndMimeType())
        return nil;

    return ::QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType);
}

NSSet *WebCore::QLPreviewGetSupportedMIMETypesSet()
{
    if (!canLoadQLPreviewGetSupportedMIMETypes())
        return nil;

    static NSSet *set = adoptNS(::QLPreviewGetSupportedMIMETypes()).leakRef();
    return set;
}

NSString *WebCore::QLTypeCopyUTIForURLAndMimeType(NSURL *url, NSString *mimeType)
{
    if (!canLoadQLTypeCopyUTIForURLAndMimeType())
        return nil;

    return ::QLTypeCopyUTIForURLAndMimeType(url, mimeType);
}

NSDictionary *WebCore::QLFileAttributes()
{
    // Set file perms to owner read/write only
    NSNumber *filePOSIXPermissions = [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE)];
    static NSDictionary *dictionary = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
                                        NSUserName(), NSFileOwnerAccountName,
                                        filePOSIXPermissions, NSFilePosixPermissions,
                                        nullptr]).leakRef();
    return dictionary;
}

NSDictionary *WebCore::QLDirectoryAttributes()
{
    // Set file perms to owner read/write/execute only
    NSNumber *directoryPOSIXPermissions = [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)];
    static NSDictionary *dictionary = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
                                                NSUserName(), NSFileOwnerAccountName,
                                                directoryPOSIXPermissions, NSFilePosixPermissions,
                                                nullptr]).leakRef();
    return dictionary;
}

static Mutex& qlPreviewConverterDictionaryMutex()
{
    static NeverDestroyed<Mutex> mutex;
    return mutex;
}

static NSMutableDictionary *QLPreviewConverterDictionary()
{
    static NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] init];
    return dictionary;
}

static NSMutableDictionary *QLContentDictionary()
{
    static NSMutableDictionary *contentDictionary = [[NSMutableDictionary alloc] init];
    return contentDictionary;
}

void WebCore::addQLPreviewConverterWithFileForURL(NSURL *url, id converter, NSString *fileName)
{
    ASSERT(url);
    ASSERT(converter);
    MutexLocker lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() setObject:converter forKey:url];
    [QLContentDictionary() setObject:(fileName ? fileName : @"") forKey:url];
}

NSString *WebCore::qlPreviewConverterFileNameForURL(NSURL *url)
{
    return [QLContentDictionary() objectForKey:url];
}

NSString *WebCore::qlPreviewConverterUTIForURL(NSURL *url)
{
    id converter = nil;
    {
        MutexLocker lock(qlPreviewConverterDictionaryMutex());
        converter = [QLPreviewConverterDictionary() objectForKey:url];
    }
    if (!converter)
        return nil;
    return [converter previewUTI];
}

void WebCore::removeQLPreviewConverterForURL(NSURL *url)
{
    MutexLocker lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() removeObjectForKey:url];

    // Delete the file when we remove the preview converter
    NSString *filename = qlPreviewConverterFileNameForURL(url);
    if ([filename length])
        [[NSFileManager defaultManager] _web_removeFileOnlyAtPath:filename];
    [QLContentDictionary() removeObjectForKey:url];
}

PassOwnPtr<ResourceRequest> WebCore::registerQLPreviewConverterIfNeeded(NSURL *url, NSString *mimeType, NSData *data)
{
    RetainPtr<NSString> updatedMIMEType = adoptNS(WebCore::QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType));

    if ([WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:updatedMIMEType.get()]) {
        RetainPtr<NSString> uti = adoptNS(WebCore::QLTypeCopyUTIForURLAndMimeType(url, updatedMIMEType.get()));

        RetainPtr<id> converter = adoptNS([[QLPreviewConverterClass() alloc] initWithData:data name:nil uti:uti.get() options:nil]);
        NSURLRequest *request = [converter previewRequest];

        // We use [request URL] here instead of url since it will be
        // the URL that the WebDataSource will see during -dealloc.
        addQLPreviewConverterWithFileForURL([request URL], converter.get(), nil);

        return adoptPtr(new ResourceRequest(request));
    }

    return nullptr;
}

const URL WebCore::safeQLURLForDocumentURLAndResourceURL(const URL& documentURL, const String& resourceURL)
{
    id converter = nil;
    NSURL *nsDocumentURL = documentURL;
    {
        MutexLocker lock(qlPreviewConverterDictionaryMutex());
        converter = [QLPreviewConverterDictionary() objectForKey:nsDocumentURL];
    }

    if (!converter)
        return URL(ParsedURLString, resourceURL);

    RetainPtr<NSURLRequest> request = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:resourceURL]]);
    NSURLRequest *safeRequest = [converter safeRequestForRequest:request.get()];
    return [safeRequest URL];
}

static Vector<char> createQLPreviewProtocol()
{
    Vector<char> previewProtocol;
#define QLPreviewScheme getQLPreviewScheme()
    const char* qlPreviewScheme = [QLPreviewScheme UTF8String];
#undef QLPreviewScheme
    previewProtocol.append(qlPreviewScheme, strlen(qlPreviewScheme) + 1);
    return previewProtocol;
}

const char* WebCore::QLPreviewProtocol()
{
    if (!canLoadQLPreviewScheme())
        return "";

    static NeverDestroyed<Vector<char>> previewProtocol(createQLPreviewProtocol());
    return previewProtocol.get().data();
}

#if USE(CFNETWORK)
// The way QuickLook works is we pass it an NSURLConnectionDelegate callback object at creation
// time. Then we pass it all the data as we receive it. Once we've downloaded the full URL,
// QuickLook turns around and send us, through this delegate, the HTML version of the file which we
// pass on to WebCore. The flag m_finishedLoadingDataIntoConverter in QuickLookHandle decides
// whether to pass the data to QuickLook or WebCore.
//
// This works fine when using NS APIs, but when using CFNetwork, we don't have a NSURLConnectionDelegate.
// So we create WebQuickLookHandleAsDelegate as an intermediate delegate object and pass it to
// QLPreviewConverter. The proxy delegate then forwards the messages on to the CFNetwork code.
@interface WebQuickLookHandleAsDelegate : NSObject <NSURLConnectionDelegate> {
    RefPtr<SynchronousResourceHandleCFURLConnectionDelegate> m_connectionDelegate;
}

- (id)initWithConnectionDelegate:(SynchronousResourceHandleCFURLConnectionDelegate*)connectionDelegate;
- (void)clearHandle;
@end

@implementation WebQuickLookHandleAsDelegate
- (id)initWithConnectionDelegate:(SynchronousResourceHandleCFURLConnectionDelegate*)connectionDelegate
{
    self = [super init];
    if (!self)
        return nil;
    m_connectionDelegate = connectionDelegate;
    return self;
}

- (void)connection:(NSURLConnection *)connection didReceiveDataArray:(NSArray *)dataArray
{
    UNUSED_PARAM(connection);
    if (!m_connectionDelegate)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didReceiveDataArray()");
    m_connectionDelegate->didReceiveDataArray(reinterpret_cast<CFArrayRef>(dataArray));
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);
    if (!m_connectionDelegate)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didReceiveData() - data length = %ld", (long)[data length]);

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (![data length])
        return;
    m_connectionDelegate->didReceiveData(reinterpret_cast<CFDataRef>(data), static_cast<int>(lengthReceived));
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);
    if (!m_connectionDelegate)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didFinishLoading()");
    m_connectionDelegate->didFinishLoading();
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);
    if (!m_connectionDelegate)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didFail()");
    m_connectionDelegate->didFail(reinterpret_cast<CFErrorRef>(error));
}

- (void)clearHandle
{
    m_connectionDelegate = nullptr;
}
@end
#endif

@interface WebResourceLoaderQuickLookDelegate : NSObject <NSURLConnectionDelegate> {
    RefPtr<ResourceLoader> _resourceLoader;
    BOOL _hasSentDidReceiveResponse;
    BOOL _hasFailed;
}
@property (nonatomic) QuickLookHandle* quickLookHandle;
@end

@implementation WebResourceLoaderQuickLookDelegate

- (id)initWithResourceLoader:(PassRefPtr<ResourceLoader>)resourceLoader
{
    self = [super init];
    if (!self)
        return nil;

    _resourceLoader = resourceLoader;
    return self;
}

- (void)_sendDidReceiveResponseIfNecessary
{
    if (_hasSentDidReceiveResponse || _hasFailed || !_quickLookHandle)
        return;

    // QuickLook might fail to convert a document without calling connection:didFailWithError: (see <rdar://problem/17927972>).
    // A nil MIME type is an indication of such a failure, so stop loading the resource and ignore subsequent delegate messages.
    NSURLResponse *previewResponse = _quickLookHandle->nsResponse();
    if (![previewResponse MIMEType]) {
        _hasFailed = YES;
        _resourceLoader->didFail(_resourceLoader->cannotShowURLError());
        return;
    }

    _hasSentDidReceiveResponse = YES;
    _resourceLoader->didReceiveResponse(previewResponse);
}

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
- (void)connection:(NSURLConnection *)connection didReceiveDataArray:(NSArray *)dataArray
{
    UNUSED_PARAM(connection);
    if (!_resourceLoader)
        return;

    [self _sendDidReceiveResponseIfNecessary];
    if (_hasFailed)
        return;

    _resourceLoader->didReceiveDataArray(reinterpret_cast<CFArrayRef>(dataArray));
}
#endif

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);
    if (!_resourceLoader)
        return;

    [self _sendDidReceiveResponseIfNecessary];
    if (_hasFailed)
        return;

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (![data length])
        return;
    _resourceLoader->didReceiveData(reinterpret_cast<const char*>([data bytes]), [data length], lengthReceived, DataPayloadBytes);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);
    if (!_resourceLoader || _hasFailed)
        return;

    ASSERT(_hasSentDidReceiveResponse);
    _resourceLoader->didFinishLoading(0);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);

    [self _sendDidReceiveResponseIfNecessary];
    if (_hasFailed)
        return;

    _resourceLoader->didFail(ResourceError(error));
}

- (void)clearHandle
{
    _resourceLoader = nullptr;
    _quickLookHandle = nullptr;
}

@end

namespace WebCore {

NSString *createTemporaryFileForQuickLook(NSString *fileName)
{
    NSString *downloadDirectory = createTemporaryDirectory(@"QuickLookContent");
    if (!downloadDirectory)
        return nil;

    NSString *contentPath = [downloadDirectory stringByAppendingPathComponent:fileName];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *uniqueContentPath = [fileManager _web_pathWithUniqueFilenameForPath:contentPath];

    BOOL success = [fileManager _web_createFileAtPathWithIntermediateDirectories:uniqueContentPath
                                                                        contents:nil
                                                                      attributes:QLFileAttributes()
                                                             directoryAttributes:QLDirectoryAttributes()];

    return success ? uniqueContentPath : nil;
}

static inline QuickLookHandleClient* emptyClient()
{
    static NeverDestroyed<QuickLookHandleClient> emptyClient;
    return &emptyClient.get();
}

QuickLookHandle::QuickLookHandle(NSURL *firstRequestURL, NSURLConnection *connection, NSURLResponse *nsResponse, id delegate)
    : m_firstRequestURL(firstRequestURL)
    , m_converter(adoptNS([[QLPreviewConverterClass() alloc] initWithConnection:connection delegate:delegate response:nsResponse options:nil]))
    , m_delegate(delegate)
    , m_finishedLoadingDataIntoConverter(false)
    , m_nsResponse([m_converter previewResponse])
    , m_client(emptyClient())
{
    LOG(Network, "QuickLookHandle::QuickLookHandle() - previewFileName: %s", [m_converter previewFileName]);
}

std::unique_ptr<QuickLookHandle> QuickLookHandle::create(ResourceHandle* handle, NSURLConnection *connection, NSURLResponse *nsResponse, id delegate)
{
    ASSERT_ARG(handle, handle);
    if (!handle->firstRequest().deprecatedIsMainResourceRequest() || ![WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:[nsResponse MIMEType]])
        return nullptr;

    std::unique_ptr<QuickLookHandle> quickLookHandle(new QuickLookHandle([handle->firstRequest().nsURLRequest(DoNotUpdateHTTPBody) URL], connection, nsResponse, delegate));
    handle->client()->didCreateQuickLookHandle(*quickLookHandle);
    return WTF::move(quickLookHandle);
}

#if USE(CFNETWORK)
std::unique_ptr<QuickLookHandle> QuickLookHandle::create(ResourceHandle* handle, SynchronousResourceHandleCFURLConnectionDelegate* connectionDelegate, CFURLResponseRef cfResponse)
{
    ASSERT_ARG(handle, handle);
    if (!handle->firstRequest().deprecatedIsMainResourceRequest() || ![WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:(NSString *)CFURLResponseGetMIMEType(cfResponse)])
        return nullptr;

    NSURLResponse *nsResponse = [NSURLResponse _responseWithCFURLResponse:cfResponse];
    WebQuickLookHandleAsDelegate *delegate = [[[WebQuickLookHandleAsDelegate alloc] initWithConnectionDelegate:connectionDelegate] autorelease];
    std::unique_ptr<QuickLookHandle> quickLookHandle(new QuickLookHandle([handle->firstRequest().nsURLRequest(DoNotUpdateHTTPBody) URL], nil, nsResponse, delegate));
    handle->client()->didCreateQuickLookHandle(*quickLookHandle);
    return WTF::move(quickLookHandle);
}

CFURLResponseRef QuickLookHandle::cfResponse()
{
    return [m_nsResponse _CFURLResponse];
}
#endif

static inline bool isMainResourceLoader(ResourceLoader* loader)
{
    return loader->documentLoader()->mainResourceLoader() == loader;
}

std::unique_ptr<QuickLookHandle> QuickLookHandle::create(ResourceLoader* loader, NSURLResponse *response)
{
    ASSERT_ARG(loader, loader);
    if (!isMainResourceLoader(loader) || ![WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:[response MIMEType]])
        return nullptr;

    RetainPtr<WebResourceLoaderQuickLookDelegate> delegate = adoptNS([[WebResourceLoaderQuickLookDelegate alloc] initWithResourceLoader:loader]);
    std::unique_ptr<QuickLookHandle> quickLookHandle(new QuickLookHandle([loader->originalRequest().nsURLRequest(DoNotUpdateHTTPBody) URL], nil, response, delegate.get()));
    [delegate setQuickLookHandle:quickLookHandle.get()];
    loader->didCreateQuickLookHandle(*quickLookHandle);
    return WTF::move(quickLookHandle);
}

NSURLResponse *QuickLookHandle::nsResponse()
{
    return m_nsResponse.get();
}

bool QuickLookHandle::didReceiveDataArray(CFArrayRef cfDataArray)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "QuickLookHandle::didReceiveDataArray()");
    [m_converter appendDataArray:(NSArray *)cfDataArray];
    m_client->didReceiveDataArray(cfDataArray);
    return true;
}

bool QuickLookHandle::didReceiveData(CFDataRef cfData)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;
    
    return didReceiveDataArray(adoptCF(CFArrayCreate(kCFAllocatorDefault, (const void**)&cfData, 1, &kCFTypeArrayCallBacks)).get());
}

bool QuickLookHandle::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "QuickLookHandle::didFinishLoading()");
    m_finishedLoadingDataIntoConverter = YES;
    [m_converter finishedAppendingData];
    m_client->didFinishLoading();
    return true;
}

void QuickLookHandle::didFail()
{
    LOG(Network, "QuickLookHandle::didFail()");
    m_client->didFail();
    [m_converter finishConverting];
    m_converter = nullptr;
}

QuickLookHandle::~QuickLookHandle()
{
    LOG(Network, "QuickLookHandle::~QuickLookHandle()");
    m_converter = nullptr;

    [m_delegate clearHandle];
}

String QuickLookHandle::previewFileName() const
{
    return [m_converter previewFileName];
}

String QuickLookHandle::previewUTI() const
{
    return [m_converter previewUTI];
}

NSURL *QuickLookHandle::previewRequestURL() const
{
    return [[m_converter previewRequest] URL];
}

}

#endif // USE(QUICK_LOOK)
