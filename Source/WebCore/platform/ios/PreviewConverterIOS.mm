/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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
#import "PreviewConverter.h"

#if ENABLE(PREVIEW_CONVERTER) && USE(QUICK_LOOK)

#import "PreviewConverterClient.h"
#import "PreviewConverterProvider.h"
#import "QuickLook.h"
#import "ResourceError.h"
#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import <pal/ios/QuickLookSoftLink.h>

@interface WebPreviewConverterDelegate : NSObject
- (instancetype)initWithDelegate:(WebCore::PreviewPlatformDelegate&)delegate;
@end

@implementation WebPreviewConverterDelegate {
    WeakPtr<WebCore::PreviewPlatformDelegate> _delegate;
}

- (instancetype)initWithDelegate:(WebCore::PreviewPlatformDelegate&)delegate
{
    if (!(self = [super init]))
        return nil;

    _delegate = makeWeakPtr(delegate);
    return self;
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    ASSERT_UNUSED(connection, !connection);
    ASSERT_UNUSED(lengthReceived, lengthReceived >= 0);
    ASSERT(data.length == static_cast<NSUInteger>(lengthReceived));
    if (auto delegate = _delegate.get())
        delegate->delegateDidReceiveData(WebCore::SharedBuffer::create(data).get());
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    ASSERT_UNUSED(connection, !connection);
    if (auto delegate = _delegate.get())
        delegate->delegateDidFinishLoading();
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    ASSERT_UNUSED(connection, !connection);
    if (auto delegate = _delegate.get())
        delegate->delegateDidFailWithError(error);
}

@end

namespace WebCore {

PreviewConverter::PreviewConverter(const ResourceResponse& response, PreviewConverterProvider& provider)
    : m_previewData { SharedBuffer::create() }
    , m_originalResponse { response }
    , m_provider { makeWeakPtr(provider) }
    , m_platformDelegate { adoptNS([[WebPreviewConverterDelegate alloc] initWithDelegate:*this]) }
    , m_platformConverter { adoptNS([PAL::allocQLPreviewConverterInstance() initWithConnection:nil delegate:m_platformDelegate.get() response:m_originalResponse.nsURLResponse() options:nil]) }
{
}

HashSet<String, ASCIICaseInsensitiveHash> PreviewConverter::platformSupportedMIMETypes()
{
    HashSet<String, ASCIICaseInsensitiveHash> supportedMIMETypes;
    for (NSString *mimeType in QLPreviewGetSupportedMIMETypesSet())
        supportedMIMETypes.add(mimeType);
    return supportedMIMETypes;
}

ResourceRequest PreviewConverter::safeRequest(const ResourceRequest& request) const
{
    return [m_platformConverter safeRequestForRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)];
}

ResourceResponse PreviewConverter::platformPreviewResponse() const
{
    ResourceResponse response { [m_platformConverter previewResponse] };
    ASSERT(response.url().protocolIs(QLPreviewProtocol));
    return response;
}

String PreviewConverter::previewFileName() const
{
    return [m_platformConverter previewFileName];
}

String PreviewConverter::previewUTI() const
{
    return [m_platformConverter previewUTI];
}

void PreviewConverter::platformAppend(const SharedBufferDataView& data)
{
    [m_platformConverter appendData:data.createNSData().get()];
}

void PreviewConverter::platformFinishedAppending()
{
    [m_platformConverter finishedAppendingData];
}

void PreviewConverter::platformFailedAppending()
{
    [m_platformConverter finishConverting];
}

bool PreviewConverter::isPlatformPasswordError(const ResourceError& error) const
{
    return error.errorCode() == kQLReturnPasswordProtected && error.domain() == "QuickLookErrorDomain";
}

static NSDictionary *optionsWithPassword(const String& password)
{
    if (password.isNull())
        return nil;
    
    return @{ (NSString *)PAL::get_QuickLook_kQLPreviewOptionPasswordKey() : password };
}

void PreviewConverter::platformUnlockWithPassword(const String& password)
{
    m_platformConverter = adoptNS([PAL::allocQLPreviewConverterInstance() initWithConnection:nil delegate:m_platformDelegate.get() response:m_originalResponse.nsURLResponse() options:optionsWithPassword(password)]);
}

} // namespace WebCore

#endif // ENABLE(PREVIEW_CONVERTER) && USE(QUICK_LOOK)
