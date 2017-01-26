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

#import "FileSystemIOS.h"
#import "Logging.h"
#import "NSFileManagerSPI.h"
#import "QuickLookHandleClient.h"
#import "ResourceError.h"
#import "ResourceLoader.h"
#import "SharedBuffer.h"
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

static QuickLookHandleClient& testingOrEmptyClient()
{
    if (testingClient())
        return *testingClient();
    return emptyClient();
}

@interface WebPreviewConverter : NSObject {
    RefPtr<ResourceLoader> _resourceLoader;
    QuickLookHandle* _handle;
    RefPtr<QuickLookHandleClient> _client;
    RetainPtr<NSURLResponse> _originalResponse;
    RetainPtr<QLPreviewConverter> _platformConverter;
    RetainPtr<NSURLResponse> _previewResponse;
    RetainPtr<NSMutableArray> _bufferedDataArray;
    BOOL _hasSentDidReceiveResponse;
    BOOL _hasFailed;
}

- (instancetype)initWithResourceLoader:(ResourceLoader&)resourceLoader resourceResponse:(const ResourceResponse&)resourceResponse quickLookHandle:(QuickLookHandle&)quickLookHandle;
- (void)setClient:(Ref<QuickLookHandleClient>&&)client;
- (void)appendDataArray:(NSArray<NSData *> *)dataArray;
- (void)finishedAppending;
- (void)failed;

@property (nonatomic, readonly) QLPreviewConverter *platformConverter;

@end

@implementation WebPreviewConverter

- (instancetype)initWithResourceLoader:(ResourceLoader&)resourceLoader resourceResponse:(const ResourceResponse&)resourceResponse quickLookHandle:(QuickLookHandle&)quickLookHandle
{
    self = [super init];
    if (!self)
        return nil;

    _resourceLoader = &resourceLoader;
    _handle = &quickLookHandle;
    _client = &testingOrEmptyClient();
    _originalResponse = resourceResponse.nsURLResponse();
    _platformConverter = adoptNS([allocQLPreviewConverterInstance() initWithConnection:nil delegate:self response:_originalResponse.get() options:nil]);
    _previewResponse = [_platformConverter previewResponse];
    _bufferedDataArray = adoptNS([[NSMutableArray alloc] init]);

    LOG(Network, "WebPreviewConverter created with preview file name \"%s\".", [_platformConverter previewFileName]);
    return self;
}

- (void)setClient:(Ref<QuickLookHandleClient>&&)client
{
    if (!testingClient())
        _client = WTFMove(client);
}

- (void)appendDataArray:(NSArray<NSData *> *)dataArray
{
    LOG(Network, "WebPreviewConverter appending data array with count %ld.", dataArray.count);
    [_platformConverter appendDataArray:dataArray];
    [_bufferedDataArray addObjectsFromArray:dataArray];
    _client->didReceiveDataArray((CFArrayRef)dataArray);
}

- (void)finishedAppending
{
    LOG(Network, "WebPreviewConverter finished appending data.");
    [_platformConverter finishedAppendingData];
    _client->didFinishLoading();
}

- (void)failed
{
    LOG(Network, "WebPreviewConverter failed.");
    [_platformConverter finishConverting];
    _client->didFail();
}

- (QLPreviewConverter *)platformConverter
{
    return _platformConverter.get();
}

- (void)_sendDidReceiveResponseIfNecessary
{
    [_bufferedDataArray removeAllObjects];

    if (_hasSentDidReceiveResponse || _hasFailed)
        return;

    // QuickLook might fail to convert a document without calling connection:didFailWithError: (see <rdar://problem/17927972>).
    // A nil MIME type is an indication of such a failure, so stop loading the resource and ignore subsequent delegate messages.
    if (![_previewResponse MIMEType]) {
        _hasFailed = YES;
        _resourceLoader->didFail(_resourceLoader->cannotShowURLError());
        return;
    }

    ResourceResponse response { _previewResponse.get() };
    response.setIsQuickLook(true);

    _hasSentDidReceiveResponse = YES;
    _resourceLoader->didReceiveResponse(response);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    ASSERT_UNUSED(connection, !connection);
    [self _sendDidReceiveResponseIfNecessary];
    if (_hasFailed)
        return;

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (auto dataLength = data.length)
        _resourceLoader->didReceiveData(reinterpret_cast<const char*>(data.bytes), dataLength, lengthReceived, DataPayloadBytes);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    ASSERT_UNUSED(connection, !connection);
    if (_hasFailed)
        return;

    ASSERT(_hasSentDidReceiveResponse);
    _resourceLoader->didFinishLoading(0);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    ASSERT_UNUSED(connection, !connection);

    if (error.code == kQLReturnPasswordProtected && [error.domain isEqualToString:@"QuickLookErrorDomain"]) {
        if (!_client->supportsPasswordEntry()) {
            _resourceLoader->didFail(_resourceLoader->cannotShowURLError());
            return;
        }

        _client->didRequestPassword([retainedSelf = retainPtr(self)] (const String& password) {
            NSDictionary *passwordOption = @{ (NSString *)kQLPreviewOptionPasswordKey : password };
            auto converterWithPassword = adoptNS([allocQLPreviewConverterInstance() initWithConnection:nil delegate:retainedSelf.get() response:retainedSelf->_originalResponse.get() options:passwordOption]);
            [converterWithPassword appendDataArray:retainedSelf->_bufferedDataArray.get()];
            [converterWithPassword finishedAppendingData];
            retainedSelf->_previewResponse = [converterWithPassword previewResponse];
            retainedSelf->_platformConverter = WTFMove(converterWithPassword);
        });
        return;
    }

    [self _sendDidReceiveResponseIfNecessary];
    if (!_hasFailed)
        _resourceLoader->didFail(error);
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
    : m_firstRequestURL { loader.originalRequest().nsURLRequest(DoNotUpdateHTTPBody).URL }
    , m_converter { adoptNS([[WebPreviewConverter alloc] initWithResourceLoader:loader resourceResponse:response quickLookHandle:*this]) }
{
    loader.didCreateQuickLookHandle(*this);
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

    [m_converter appendDataArray:@[ [NSData dataWithBytes:data length:length] ]];
    return true;
}

bool QuickLookHandle::didReceiveBuffer(const SharedBuffer& buffer)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    [m_converter appendDataArray:buffer.createNSDataArray().get()];
    return true;
}

bool QuickLookHandle::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    m_finishedLoadingDataIntoConverter = true;
    [m_converter finishedAppending];
    return true;
}

void QuickLookHandle::didFail()
{
    [m_converter failed];
    m_converter = nullptr;
}

void QuickLookHandle::setClient(Ref<QuickLookHandleClient>&& client)
{
    [m_converter setClient:WTFMove(client)];
}

void QuickLookHandle::setClientForTesting(RefPtr<QuickLookHandleClient>&& client)
{
    testingClient() = WTFMove(client);
}

String QuickLookHandle::previewFileName() const
{
    return [m_converter platformConverter].previewFileName;
}

String QuickLookHandle::previewUTI() const
{
    return [m_converter platformConverter].previewUTI;
}

NSURL *QuickLookHandle::previewRequestURL() const
{
    return [m_converter platformConverter].previewRequest.URL;
}

QLPreviewConverter *QuickLookHandle::converter() const
{
    return [m_converter platformConverter];
}

}

#endif // USE(QUICK_LOOK)
