/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "Logging.h"
#import "NSFileManagerSPI.h"
#import "PreviewConverter.h"
#import "QuickLookHandleClient.h"
#import "ResourceError.h"
#import "ResourceLoader.h"
#import "ResourceRequest.h"
#import "SchemeRegistry.h"
#import "SharedBuffer.h"
#import <WebCore/NetworkLoadMetrics.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

#import "QuickLookSoftLink.h"

using namespace WebCore;

NSSet *WebCore::QLPreviewGetSupportedMIMETypesSet()
{
    static NeverDestroyed<RetainPtr<NSSet>> set = QLPreviewGetSupportedMIMETypes();
    return set.get().get();
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

void WebCore::removeQLPreviewConverterForURL(NSURL *url)
{
    LockHolder lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() removeObjectForKey:url];
    [QLContentDictionary() removeObjectForKey:url];
}

static void addQLPreviewConverterWithFileForURL(NSURL *url, id converter, NSString *fileName)
{
    ASSERT(url);
    ASSERT(converter);
    LockHolder lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() setObject:converter forKey:url];
    [QLContentDictionary() setObject:(fileName ? fileName : @"") forKey:url];
}

RetainPtr<NSURLRequest> WebCore::registerQLPreviewConverterIfNeeded(NSURL *url, NSString *mimeType, NSData *data)
{
    RetainPtr<NSString> updatedMIMEType = adoptNS(QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType));

    if ([QLPreviewGetSupportedMIMETypesSet() containsObject:updatedMIMEType.get()]) {
        RetainPtr<NSString> uti = adoptNS(QLTypeCopyUTIForURLAndMimeType(url, updatedMIMEType.get()));

        auto converter = std::make_unique<PreviewConverter>(data, uti.get());
        ResourceRequest previewRequest = converter->previewRequest();

        // We use [request URL] here instead of url since it will be
        // the URL that the WebDataSource will see during -dealloc.
        addQLPreviewConverterWithFileForURL(previewRequest.url(), converter->platformConverter(), nil);

        return previewRequest.nsURLRequest(DoNotUpdateHTTPBody);
    }

    return nil;
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

bool WebCore::isQuickLookPreviewURL(const URL& url)
{
    // Use some known protocols as a short-cut to avoid loading the QuickLook framework.
    if (url.protocolIsInHTTPFamily() || url.isBlankURL() || url.protocolIsBlob() || url.protocolIsData() || SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol().toString()))
        return NO;
    return url.protocolIs(QLPreviewProtocol());
}

static RefPtr<QuickLookHandleClient>& testingClient()
{
    static NeverDestroyed<RefPtr<QuickLookHandleClient>> testingClient;
    return testingClient.get();
}

static QuickLookHandleClient& emptyClient()
{
    static NeverDestroyed<QuickLookHandleClient> emptyClient;
    return emptyClient.get();
}

@interface WebPreviewLoader : NSObject {
    RefPtr<ResourceLoader> _resourceLoader;
    ResourceResponse _response;
    QuickLookHandle* _handle;
    RefPtr<QuickLookHandleClient> _client;
    std::unique_ptr<PreviewConverter> _converter;
    RetainPtr<NSMutableArray> _bufferedDataArray;
    BOOL _hasSentDidReceiveResponse;
}

- (instancetype)initWithResourceLoader:(ResourceLoader&)resourceLoader resourceResponse:(const ResourceResponse&)resourceResponse quickLookHandle:(QuickLookHandle&)quickLookHandle;
- (void)appendDataArray:(NSArray<NSData *> *)dataArray;
- (void)finishedAppending;
- (void)failed;

@end

@implementation WebPreviewLoader

- (instancetype)initWithResourceLoader:(ResourceLoader&)resourceLoader resourceResponse:(const ResourceResponse&)resourceResponse quickLookHandle:(QuickLookHandle&)quickLookHandle
{
    self = [super init];
    if (!self)
        return nil;

    _resourceLoader = &resourceLoader;
    _response = resourceResponse;
    _handle = &quickLookHandle;
    _converter = std::make_unique<PreviewConverter>(self, _response);
    _bufferedDataArray = adoptNS([[NSMutableArray alloc] init]);

    if (testingClient())
        _client = testingClient();
    else if (auto client = resourceLoader.frameLoader()->client().createQuickLookHandleClient(_converter->previewFileName(), _converter->previewUTI()))
        _client = WTFMove(client);
    else
        _client = &emptyClient();

    LOG(Network, "WebPreviewConverter created with preview file name \"%s\".", _converter->previewFileName().utf8().data());
    return self;
}

- (void)appendDataArray:(NSArray<NSData *> *)dataArray
{
    LOG(Network, "WebPreviewConverter appending data array with count %ld.", dataArray.count);
    [_converter->platformConverter() appendDataArray:dataArray];
    [_bufferedDataArray addObjectsFromArray:dataArray];
    _client->didReceiveDataArray((CFArrayRef)dataArray);
}

- (void)finishedAppending
{
    LOG(Network, "WebPreviewConverter finished appending data.");
    [_converter->platformConverter() finishedAppendingData];
    _client->didFinishLoading();
}

- (void)failed
{
    LOG(Network, "WebPreviewConverter failed.");
    [_converter->platformConverter() finishConverting];
    _client->didFail();
}

- (void)_sendDidReceiveResponseIfNecessary
{
    if (_hasSentDidReceiveResponse)
        return;

    [_bufferedDataArray removeAllObjects];

    ResourceResponse response { _converter->previewResponse() };
    response.setIsQuickLook(true);
    ASSERT(response.mimeType().length());

    _resourceLoader->documentLoader()->setPreviewConverter(WTFMove(_converter));

    _hasSentDidReceiveResponse = YES;
    _resourceLoader->didReceiveResponse(response);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    ASSERT_UNUSED(connection, !connection);
    [self _sendDidReceiveResponseIfNecessary];

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (auto dataLength = data.length)
        _resourceLoader->didReceiveData(reinterpret_cast<const char*>(data.bytes), dataLength, lengthReceived, DataPayloadBytes);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    ASSERT_UNUSED(connection, !connection);
    ASSERT(_hasSentDidReceiveResponse);

    NetworkLoadMetrics emptyMetrics;
    _resourceLoader->didFinishLoading(emptyMetrics);
}

static inline bool isQuickLookPasswordError(NSError *error)
{
    return error.code == kQLReturnPasswordProtected && [error.domain isEqualToString:@"QuickLookErrorDomain"];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    ASSERT_UNUSED(connection, !connection);

    if (!isQuickLookPasswordError(error)) {
        [self _sendDidReceiveResponseIfNecessary];
        _resourceLoader->didFail(error);
        return;
    }

    if (!_client->supportsPasswordEntry()) {
        _resourceLoader->didFail(_resourceLoader->cannotShowURLError());
        return;
    }

    _client->didRequestPassword([retainedSelf = retainPtr(self)] (const String& password) {
        auto converter = std::make_unique<PreviewConverter>(retainedSelf.get(), retainedSelf->_response, password);
        QLPreviewConverter *platformConverter = converter->platformConverter();
        [platformConverter appendDataArray:retainedSelf->_bufferedDataArray.get()];
        [platformConverter finishedAppendingData];
        retainedSelf->_converter = WTFMove(converter);
    });
}

@end

namespace WebCore {

static NSDictionary *temporaryFileAttributes()
{
    static NSDictionary *attributes = [@{
        NSFileOwnerAccountName : NSUserName(),
        NSFilePosixPermissions : [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE)],
        } retain];
    return attributes;
}

static NSDictionary *temporaryDirectoryAttributes()
{
    static NSDictionary *attributes = [@{
        NSFileOwnerAccountName : NSUserName(),
        NSFilePosixPermissions : [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)],
        NSFileProtectionKey : NSFileProtectionCompleteUnlessOpen,
        } retain];
    return attributes;
}

NSString *createTemporaryFileForQuickLook(NSString *fileName)
{
    NSString *downloadDirectory = createTemporaryDirectory(@"QuickLookContent");
    if (!downloadDirectory)
        return nil;

    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSError *error;
    if (![fileManager setAttributes:temporaryDirectoryAttributes() ofItemAtPath:downloadDirectory error:&error]) {
        LOG_ERROR("Failed to set attribute NSFileProtectionCompleteUnlessOpen on directory %@ with error: %@.", downloadDirectory, error.localizedDescription);
        return nil;
    }

    NSString *contentPath = [downloadDirectory stringByAppendingPathComponent:fileName.lastPathComponent];
    if (![fileManager _web_createFileAtPath:contentPath contents:nil attributes:temporaryFileAttributes()]) {
        LOG_ERROR("Failed to create QuickLook temporary file at path %@.", contentPath);
        return nil;
    }

    return contentPath;
}

QuickLookHandle::QuickLookHandle(ResourceLoader& loader, const ResourceResponse& response)
    : m_previewLoader { adoptNS([[WebPreviewLoader alloc] initWithResourceLoader:loader resourceResponse:response quickLookHandle:*this]) }
{
}

QuickLookHandle::~QuickLookHandle()
{
}

bool QuickLookHandle::shouldCreateForMIMEType(const String& mimeType)
{
    return [QLPreviewGetSupportedMIMETypesSet() containsObject:mimeType];
}

std::unique_ptr<QuickLookHandle> QuickLookHandle::create(ResourceLoader& loader, const ResourceResponse& response)
{
    ASSERT(shouldCreateForMIMEType(response.mimeType()));
    return std::make_unique<QuickLookHandle>(loader, response);
}

bool QuickLookHandle::didReceiveData(const char* data, unsigned length)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    [m_previewLoader appendDataArray:@[ [NSData dataWithBytes:data length:length] ]];
    return true;
}

bool QuickLookHandle::didReceiveBuffer(const SharedBuffer& buffer)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    [m_previewLoader appendDataArray:buffer.createNSDataArray().get()];
    return true;
}

bool QuickLookHandle::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    m_finishedLoadingDataIntoConverter = true;
    [m_previewLoader finishedAppending];
    return true;
}

void QuickLookHandle::didFail()
{
    [m_previewLoader failed];
    m_previewLoader = nullptr;
}

void QuickLookHandle::setClientForTesting(RefPtr<QuickLookHandleClient>&& client)
{
    testingClient() = WTFMove(client);
}

}

#endif // USE(QUICK_LOOK)
