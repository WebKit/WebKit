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
#import "PreviewLoader.h"

#if USE(QUICK_LOOK)

#import "DocumentLoader.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "Logging.h"
#import "PreviewConverter.h"
#import "PreviewLoaderClient.h"
#import "QuickLook.h"
#import "ResourceLoader.h"
#import <pal/spi/ios/QuickLookSPI.h>
#import <wtf/NeverDestroyed.h>

using namespace WebCore;

@interface WebPreviewLoader : NSObject {
    RefPtr<ResourceLoader> _resourceLoader;
    ResourceResponse _response;
    RefPtr<PreviewLoaderClient> _client;
    std::unique_ptr<PreviewConverter> _converter;
    RetainPtr<NSMutableArray> _bufferedDataArray;
    BOOL _hasSentDidReceiveResponse;
}

- (instancetype)initWithResourceLoader:(ResourceLoader&)resourceLoader resourceResponse:(const ResourceResponse&)resourceResponse;
- (void)appendDataArray:(NSArray<NSData *> *)dataArray;
- (void)finishedAppending;
- (void)failed;

@end

@implementation WebPreviewLoader

static RefPtr<PreviewLoaderClient>& testingClient()
{
    static NeverDestroyed<RefPtr<PreviewLoaderClient>> testingClient;
    return testingClient.get();
}

static PreviewLoaderClient& emptyClient()
{
    static NeverDestroyed<PreviewLoaderClient> emptyClient;
    return emptyClient.get();
}

- (instancetype)initWithResourceLoader:(ResourceLoader&)resourceLoader resourceResponse:(const ResourceResponse&)resourceResponse
{
    self = [super init];
    if (!self)
        return nil;

    _resourceLoader = &resourceLoader;
    _response = resourceResponse;
    _converter = std::make_unique<PreviewConverter>(self, _response);
    _bufferedDataArray = adoptNS([[NSMutableArray alloc] init]);

    if (testingClient())
        _client = testingClient();
    else if (auto client = resourceLoader.frameLoader()->client().createPreviewLoaderClient(_converter->previewFileName(), _converter->previewUTI()))
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
    ASSERT(!_resourceLoader->reachedTerminalState());
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
    if (_resourceLoader->reachedTerminalState())
        return;
    
    [self _sendDidReceiveResponseIfNecessary];

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (auto dataLength = data.length)
        _resourceLoader->didReceiveData(reinterpret_cast<const char*>(data.bytes), dataLength, lengthReceived, DataPayloadBytes);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    ASSERT_UNUSED(connection, !connection);
    if (_resourceLoader->reachedTerminalState())
        return;
    
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
    if (_resourceLoader->reachedTerminalState())
        return;

    if (!isQuickLookPasswordError(error)) {
        [self _sendDidReceiveResponseIfNecessary];
        _resourceLoader->didFail(error);
        return;
    }

    if (!_client->supportsPasswordEntry()) {
        _resourceLoader->didFail(_resourceLoader->cannotShowURLError());
        return;
    }

    _client->didRequestPassword([self, retainedSelf = retainPtr(self)] (const String& password) {
        _converter = std::make_unique<PreviewConverter>(self, _response, password);
        [_converter->platformConverter() appendDataArray:_bufferedDataArray.get()];
        [_converter->platformConverter() finishedAppendingData];
    });
}

@end

namespace WebCore {

PreviewLoader::PreviewLoader(ResourceLoader& loader, const ResourceResponse& response)
    : m_previewLoader { adoptNS([[WebPreviewLoader alloc] initWithResourceLoader:loader resourceResponse:response]) }
{
}

PreviewLoader::~PreviewLoader()
{
}

bool PreviewLoader::shouldCreateForMIMEType(const String& mimeType)
{
    static std::once_flag onceFlag;
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> supportedMIMETypes;
    std::call_once(onceFlag, [] {
        for (NSString *mimeType in QLPreviewGetSupportedMIMETypesSet())
            supportedMIMETypes.get().add(mimeType);
    });

    if (mimeType.isNull())
        return false;

    return supportedMIMETypes.get().contains(mimeType);
}

std::unique_ptr<PreviewLoader> PreviewLoader::create(ResourceLoader& loader, const ResourceResponse& response)
{
    ASSERT(shouldCreateForMIMEType(response.mimeType()));
    return std::make_unique<PreviewLoader>(loader, response);
}

bool PreviewLoader::didReceiveData(const char* data, unsigned length)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    [m_previewLoader appendDataArray:@[ [NSData dataWithBytes:data length:length] ]];
    return true;
}

bool PreviewLoader::didReceiveBuffer(const SharedBuffer& buffer)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    [m_previewLoader appendDataArray:buffer.createNSDataArray().get()];
    return true;
}

bool PreviewLoader::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    m_finishedLoadingDataIntoConverter = true;
    [m_previewLoader finishedAppending];
    return true;
}

void PreviewLoader::didFail()
{
    if (m_finishedLoadingDataIntoConverter)
        return;

    m_finishedLoadingDataIntoConverter = true;
    [m_previewLoader failed];
    m_previewLoader = nullptr;
}

void PreviewLoader::setClientForTesting(RefPtr<PreviewLoaderClient>&& client)
{
    testingClient() = WTFMove(client);
}

} // namespace WebCore

#endif // USE(QUICK_LOOK)
