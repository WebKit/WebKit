/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

// FIXME (116158267): This file can be removed and its implementation merged directly into
// CDMInstanceSessionFairPlayStreamingAVFObjC once we no logner need to support a configuration
// where the BuiltInCDMKeyGroupingStrategyEnabled preference is off.

#import "config.h"
#import "WebAVContentKeyGroup.h"

#if HAVE(AVCONTENTKEYSESSION)

#import "ContentKeyGroupDataSource.h"
#import "Logging.h"
#import "NotImplemented.h"
#import <wtf/LoggerHelper.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/WTFString.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

NS_ASSUME_NONNULL_BEGIN

#if !RELEASE_LOG_DISABLED
@interface WebAVContentKeyGroup (Logging)
@property (nonatomic, readonly) uint64_t logIdentifier;
@property (nonatomic, readonly) const Logger* loggerPtr;
@property (nonatomic, readonly) WTFLogChannel* logChannel;
@end
#endif

#if HAVE(AVCONTENTKEY_REVOKE)
// FIXME (117803793): Remove staging code once -[AVContentKey revoke] is available in SDKs used by WebKit builders
@interface AVContentKey (Staging_113340014)
- (void)revoke;
@end
#endif

@implementation WebAVContentKeyGroup {
    WeakObjCPtr<AVContentKeySession> _contentKeySession;
    WeakPtr<WebCore::ContentKeyGroupDataSource> _dataSource;
    RetainPtr<NSUUID> _groupIdentifier;
}

- (instancetype)initWithContentKeySession:(AVContentKeySession *)contentKeySession dataSource:(WebCore::ContentKeyGroupDataSource&)dataSource
{
    self = [super init];
    if (!self)
        return nil;

    _contentKeySession = contentKeySession;
    _dataSource = dataSource;
    _groupIdentifier = adoptNS([[NSUUID alloc] init]);

    OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "groupIdentifier=", _groupIdentifier.get());
    return self;
}

#pragma mark WebAVContentKeyGrouping

- (nullable NSData *)contentProtectionSessionIdentifier
{
    uuid_t uuidBytes = { };
    [_groupIdentifier getUUIDBytes:uuidBytes];
    return [NSData dataWithBytes:uuidBytes length:sizeof(uuidBytes)];
}

- (BOOL)associateContentKeyRequest:(AVContentKeyRequest *)contentKeyRequest
{
    // We assume the data source is tracking unexpected key requests and will include them when
    // ContentKeyGroupDataSource::contentKeyGroupDataSourceKeys() is called in -expire, so there's
    // no need to do any explicit association here.
    OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "contentKeyRequest=", contentKeyRequest);
    return YES;
}

- (void)expire
{
    RefPtr dataSource = _dataSource.get();
    if (!dataSource)
        return;

#if HAVE(AVCONTENTKEY_REVOKE)
    Vector keys = dataSource->contentKeyGroupDataSourceKeys();
    OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, "keys=", keys.size());

    // FIXME (117803793): Remove staging code once -[AVContentKey revoke] is available in SDKs used by WebKit builders
    if (![PAL::getAVContentKeyClassSingleton() instancesRespondToSelector:@selector(revoke)])
        return;

    for (auto& key : keys)
        [key revoke];
#endif
}

- (void)processContentKeyRequestWithIdentifier:(nullable id)identifier initializationData:(nullable NSData *)initializationData options:(nullable NSDictionary<NSString *, id> *)options
{
    OBJC_INFO_LOG(OBJC_LOGIDENTIFIER, identifier, ", initializationData=", initializationData, ", options=", options);
    [_contentKeySession processContentKeyRequestWithIdentifier:identifier initializationData:initializationData options:options];
}

@end

#if !RELEASE_LOG_DISABLED

@implementation WebAVContentKeyGroup (Logging)

- (uint64_t)logIdentifier
{
    RefPtr dataSource = _dataSource.get();
    return dataSource ? dataSource->contentKeyGroupDataSourceLogIdentifier() : 0;
}

- (const Logger*)loggerPtr
{
    RefPtr dataSource = _dataSource.get();
    return dataSource ? &dataSource->contentKeyGroupDataSourceLogger() : nullptr;
}

- (WTFLogChannel*)logChannel
{
    RefPtr dataSource = _dataSource.get();
    return dataSource ? &dataSource->contentKeyGroupDataSourceLogChannel() : nullptr;
}

@end

#endif // !RELEASE_LOG_DISABLED

NS_ASSUME_NONNULL_END

#endif // HAVE(AVCONTENTKEYSESSION)
