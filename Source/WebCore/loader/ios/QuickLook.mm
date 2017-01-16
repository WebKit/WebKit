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

@interface WebPreviewConverterDelegate : NSObject {
    RefPtr<ResourceLoader> _resourceLoader;
    QuickLookHandle* _quickLookHandle;
    BOOL _hasSentDidReceiveResponse;
    BOOL _hasFailed;
}
@end

@implementation WebPreviewConverterDelegate

- (id)initWithResourceLoader:(ResourceLoader&)resourceLoader quickLookHandle:(QuickLookHandle&)quickLookHandle
{
    self = [super init];
    if (!self)
        return nil;

    _resourceLoader = &resourceLoader;
    _quickLookHandle = &quickLookHandle;
    return self;
}

- (void)_sendDidReceiveResponseIfNecessary
{
    if (_hasSentDidReceiveResponse || _hasFailed)
        return;

    // QuickLook might fail to convert a document without calling connection:didFailWithError: (see <rdar://problem/17927972>).
    // A nil MIME type is an indication of such a failure, so stop loading the resource and ignore subsequent delegate messages.
    NSURLResponse *previewResponse = _quickLookHandle->previewResponse();
    if (!previewResponse.MIMEType) {
        _hasFailed = YES;
        _resourceLoader->didFail(_resourceLoader->cannotShowURLError());
        return;
    }

    ResourceResponse response { previewResponse };
    response.setIsQuickLook(true);

    _hasSentDidReceiveResponse = YES;
    _resourceLoader->didReceiveResponse(response);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);
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
    UNUSED_PARAM(connection);
    if (_hasFailed)
        return;

    ASSERT(_hasSentDidReceiveResponse);
    _resourceLoader->didFinishLoading(0);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);
    [self _sendDidReceiveResponseIfNecessary];
    if (!_hasFailed)
        _resourceLoader->didFail(error);
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

static inline QuickLookHandleClient& emptyClient()
{
    static auto& emptyClient = adoptRef(*new QuickLookHandleClient()).leakRef();
    return emptyClient;
}

QuickLookHandle::QuickLookHandle(ResourceLoader& loader, const ResourceResponse& response)
    : m_firstRequestURL { loader.originalRequest().nsURLRequest(DoNotUpdateHTTPBody).URL }
    , m_delegate { adoptNS([[WebPreviewConverterDelegate alloc] initWithResourceLoader:loader quickLookHandle:*this]) }
    , m_converter { adoptNS([allocQLPreviewConverterInstance() initWithConnection:nil delegate:m_delegate.get() response:response.nsURLResponse() options:nil]) }
    , m_previewResponse { [m_converter previewResponse] }
    , m_client { emptyClient() }
{
    LOG(Network, "QuickLookHandle::QuickLookHandle() - previewFileName: %s", [m_converter previewFileName]);
    loader.didCreateQuickLookHandle(*this);
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

    didReceiveDataArray(@[ [NSData dataWithBytes:data length:length] ]);
    return true;
}

bool QuickLookHandle::didReceiveBuffer(const SharedBuffer& buffer)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    didReceiveDataArray(buffer.createNSDataArray().get());
    return true;
}

void QuickLookHandle::didReceiveDataArray(NSArray *dataArray)
{
    ASSERT(!m_finishedLoadingDataIntoConverter);
    LOG(Network, "QuickLookHandle::didReceiveDataArray()");
    [m_converter appendDataArray:dataArray];
    m_client->didReceiveDataArray((CFArrayRef)dataArray);
}

bool QuickLookHandle::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "QuickLookHandle::didFinishLoading()");
    m_finishedLoadingDataIntoConverter = true;
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

void QuickLookHandle::setClient(Ref<QuickLookHandleClient>&& client)
{
    m_client = WTFMove(client);
}

QuickLookHandle::~QuickLookHandle()
{
    LOG(Network, "QuickLookHandle::~QuickLookHandle()");
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
    return [m_converter previewRequest].URL;
}

NSURLResponse *QuickLookHandle::previewResponse() const
{
    return m_previewResponse.get();
}

}

#endif // USE(QUICK_LOOK)
