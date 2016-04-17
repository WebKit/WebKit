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
#import "NSFileManagerSPI.h"
#import "ResourceError.h"
#import "ResourceHandle.h"
#import "ResourceLoader.h"
#import "RuntimeApplicationChecks.h"
#import "SubresourceLoader.h"
#import "SynchronousResourceHandleCFURLConnectionDelegate.h"
#import "UTIUtilities.h"
#import "WebCoreResourceHandleAsDelegate.h"
#import "WebCoreSystemInterface.h"
#import "WebCoreURLResponseIOS.h"
#import <Foundation/Foundation.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

#import "QuickLookSoftLink.h"

SOFT_LINK_FRAMEWORK(MobileCoreServices)

SOFT_LINK(MobileCoreServices, UTTypeCreatePreferredIdentifierForTag, CFStringRef, (CFStringRef inTagClass, CFStringRef inTag, CFStringRef inConformingToUTI), (inTagClass, inTag, inConformingToUTI))

SOFT_LINK_CONSTANT(MobileCoreServices, kUTTagClassFilenameExtension, CFStringRef)

#define kUTTagClassFilenameExtension getkUTTagClassFilenameExtension()

using namespace WebCore;

NSSet *WebCore::QLPreviewGetSupportedMIMETypesSet()
{
    static NeverDestroyed<RetainPtr<NSSet>> set = QLPreviewGetSupportedMIMETypes();
    return set.get().get();
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

static Lock& qlPreviewConverterDictionaryMutex()
{
    static NeverDestroyed<Lock> mutex;
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

// We must ensure that the MIME type is correct, so that QuickLook's web plugin is called when needed.
// We filter the basic MIME types so that we don't do unnecessary work in standard browsing situations.
static RetainPtr<CFStringRef> adjustMIMETypeForQuickLook(CFURLResponseRef cfResponse)
{
    RetainPtr<CFStringRef> mimeType = CFURLResponseGetMIMEType(cfResponse);
    if (!shouldUseQuickLookForMIMEType(mimeType.get()))
        return mimeType;

    RetainPtr<CFStringRef> suggestedFilename = adoptCF(CFURLResponseCopySuggestedFilename(cfResponse));
    RetainPtr<CFStringRef> quickLookMIMEType = adoptCF((CFStringRef)QLTypeCopyBestMimeTypeForFileNameAndMimeType((NSString *)suggestedFilename.get(), (NSString *)mimeType.get()));
    if (!quickLookMIMEType) {
        auto url = CFURLResponseGetURL(cfResponse);
        if (![(NSURL *)url isFileURL])
            return mimeType;
        RetainPtr<CFStringRef> extension = adoptCF(CFURLCopyPathExtension(url));
        if (!extension)
            return mimeType;
        RetainPtr<CFStringRef> uti = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension.get(), nullptr));
        quickLookMIMEType = mimeTypeFromUTITree(uti.get());
        if (!quickLookMIMEType)
            return mimeType;
    }

    if (!mimeType || CFStringCompare(mimeType.get(), quickLookMIMEType.get(), kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
        CFURLResponseSetMIMEType(cfResponse, quickLookMIMEType.get());
        return quickLookMIMEType;
    }

    return mimeType;
}

static bool shouldCreateForResponse(CFURLResponseRef cfResponse)
{
    RetainPtr<CFStringRef> mimeType = adjustMIMETypeForQuickLook(cfResponse);
    return [QLPreviewGetSupportedMIMETypesSet() containsObject:(NSString *)mimeType.get()];
}

void WebCore::addQLPreviewConverterWithFileForURL(NSURL *url, id converter, NSString *fileName)
{
    ASSERT(url);
    ASSERT(converter);
    LockHolder lock(qlPreviewConverterDictionaryMutex());
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
        LockHolder lock(qlPreviewConverterDictionaryMutex());
        converter = [QLPreviewConverterDictionary() objectForKey:url];
    }
    if (!converter)
        return nil;
    return [converter previewUTI];
}

void WebCore::removeQLPreviewConverterForURL(NSURL *url)
{
    LockHolder lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() removeObjectForKey:url];

    // Delete the file when we remove the preview converter
    NSString *filename = qlPreviewConverterFileNameForURL(url);
    if ([filename length])
        [[NSFileManager defaultManager] _web_removeFileOnlyAtPath:filename];
    [QLContentDictionary() removeObjectForKey:url];
}

RetainPtr<NSURLRequest> WebCore::registerQLPreviewConverterIfNeeded(NSURL *url, NSString *mimeType, NSData *data)
{
    RetainPtr<NSString> updatedMIMEType = adoptNS(QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType));

    if ([QLPreviewGetSupportedMIMETypesSet() containsObject:updatedMIMEType.get()]) {
        RetainPtr<NSString> uti = adoptNS(QLTypeCopyUTIForURLAndMimeType(url, updatedMIMEType.get()));

        RetainPtr<QLPreviewConverter> converter = adoptNS([allocQLPreviewConverterInstance() initWithData:data name:nil uti:uti.get() options:nil]);
        NSURLRequest *request = [converter previewRequest];

        // We use [request URL] here instead of url since it will be
        // the URL that the WebDataSource will see during -dealloc.
        addQLPreviewConverterWithFileForURL([request URL], converter.get(), nil);

        return request;
    }

    return nil;
}

const URL WebCore::safeQLURLForDocumentURLAndResourceURL(const URL& documentURL, const String& resourceURL)
{
    id converter = nil;
    NSURL *nsDocumentURL = documentURL;
    {
        LockHolder lock(qlPreviewConverterDictionaryMutex());
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
    const char* qlPreviewScheme = [QLPreviewScheme UTF8String];
    previewProtocol.append(qlPreviewScheme, strlen(qlPreviewScheme) + 1);
    return previewProtocol;
}

const char* WebCore::QLPreviewProtocol()
{
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
@interface WebQuickLookHandleAsDelegate : NSObject <NSURLConnectionDelegate, WebCoreResourceLoaderDelegate> {
    RefPtr<SynchronousResourceHandleCFURLConnectionDelegate> m_connectionDelegate;
}

- (id)initWithConnectionDelegate:(SynchronousResourceHandleCFURLConnectionDelegate*)connectionDelegate;
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

- (void)detachHandle
{
    m_connectionDelegate = nullptr;
}
@end
#endif

@interface WebResourceLoaderQuickLookDelegate : NSObject <NSURLConnectionDelegate, WebCoreResourceLoaderDelegate> {
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

- (void)detachHandle
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
    , m_converter(adoptNS([allocQLPreviewConverterInstance() initWithConnection:connection delegate:delegate response:nsResponse options:nil]))
    , m_delegate(delegate)
    , m_finishedLoadingDataIntoConverter(false)
    , m_nsResponse([m_converter previewResponse])
    , m_client(emptyClient())
{
    LOG(Network, "QuickLookHandle::QuickLookHandle() - previewFileName: %s", [m_converter previewFileName]);
}

static bool shouldCreate(ResourceHandle& handle, CFURLResponseRef response)
{
    return handle.firstRequest().requester() == ResourceRequest::Requester::Main && shouldCreateForResponse(response);
}

std::unique_ptr<QuickLookHandle> QuickLookHandle::create(ResourceHandle& handle, NSURLConnection *connection, NSURLResponse *response, id delegate)
{
    std::unique_ptr<QuickLookHandle> quickLookHandle(new QuickLookHandle([handle.firstRequest().nsURLRequest(DoNotUpdateHTTPBody) URL], connection, response, delegate));
    handle.client()->didCreateQuickLookHandle(*quickLookHandle);
    return quickLookHandle;
}

std::unique_ptr<QuickLookHandle> QuickLookHandle::createIfNecessary(ResourceHandle& handle, NSURLConnection *connection, NSURLResponse *response, id delegate)
{
    if (!shouldCreate(handle, response._CFURLResponse))
        return nullptr;

    return create(handle, connection, response, delegate);
}

#if USE(CFNETWORK)
std::unique_ptr<QuickLookHandle> QuickLookHandle::createIfNecessary(ResourceHandle& handle, SynchronousResourceHandleCFURLConnectionDelegate* connectionDelegate, CFURLResponseRef cfResponse)
{
    if (!shouldCreate(handle, cfResponse))
        return nullptr;

    NSURLResponse *response = [NSURLResponse _responseWithCFURLResponse:cfResponse];
    WebQuickLookHandleAsDelegate *delegate = [[[WebQuickLookHandleAsDelegate alloc] initWithConnectionDelegate:connectionDelegate] autorelease];
    return create(handle, nil, response, delegate);
}

CFURLResponseRef QuickLookHandle::cfResponse()
{
    return [m_nsResponse _CFURLResponse];
}
#endif

std::unique_ptr<QuickLookHandle> QuickLookHandle::createIfNecessary(ResourceLoader& loader, NSURLResponse *response)
{
    bool isMainResourceLoad = loader.documentLoader()->mainResourceLoader() == &loader;
    if (!isMainResourceLoad)
        return nullptr;

    if (!shouldCreateForResponse(response._CFURLResponse))
        return nullptr;

    RetainPtr<WebResourceLoaderQuickLookDelegate> delegate = adoptNS([[WebResourceLoaderQuickLookDelegate alloc] initWithResourceLoader:&loader]);
    std::unique_ptr<QuickLookHandle> quickLookHandle(new QuickLookHandle([loader.originalRequest().nsURLRequest(DoNotUpdateHTTPBody) URL], nil, response, delegate.get()));
    [delegate setQuickLookHandle:quickLookHandle.get()];
    loader.didCreateQuickLookHandle(*quickLookHandle);
    return quickLookHandle;
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

    [m_delegate detachHandle];
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
