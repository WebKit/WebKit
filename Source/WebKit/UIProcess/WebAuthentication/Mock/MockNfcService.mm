/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "MockNfcService.h"

#if ENABLE(WEB_AUTHN)
#import "CtapNfcDriver.h"
#import "NearFieldSPI.h"
#import "NfcConnection.h"
#import <WebCore/FidoConstants.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>

#import "NearFieldSoftLink.h"

#if HAVE(NEAR_FIELD)

@interface WKMockNFTag : NSObject <NFTag>

- (instancetype)initWithType:(NFTagType)type;

@end

@implementation WKMockNFTag {
    NFTagType _type;
}

@synthesize technology;
@synthesize tagID;
@synthesize AppData;
@synthesize UID;
@synthesize ndefAvailability;
@synthesize ndefMessageSize;
@synthesize ndefContainerSize;
@synthesize tagA;
@synthesize tagB;
@synthesize tagF;

- (NFTagType)type
{
    return _type;
}

- (instancetype)initWithNFTag:(id<NFTag>)tag
{
    if ((self = [super init]))
        _type = tag.type;
    return self;
}

- (NSString*)description
{
    return nil;
}

- (BOOL)isEqualToNFTag:(id<NFTag>)tag
{
    return NO;
}

- (instancetype)initWithType:(NFTagType)type
{
    if ((self = [super init]))
        _type = type;
    return self;
}

@end

#endif // HAVE(NEAR_FIELD)

namespace WebKit {
using namespace fido;
using MockNfc = MockWebAuthenticationConfiguration::Nfc;

#if HAVE(NEAR_FIELD)

namespace {

static id<NFReaderSessionDelegate> globalNFReaderSessionDelegate;
static MockNfcService* globalNfcService;

static void NFReaderSessionSetDelegate(id, SEL, id<NFReaderSessionDelegate> delegate)
{
    globalNFReaderSessionDelegate = delegate;
}

static BOOL NFReaderSessionConnectTagFail(id, SEL, NFTag *)
{
    return NO;
}

static BOOL NFReaderSessionConnectTag(id, SEL, NFTag *)
{
    return YES;
}

static NSData* NFReaderSessionTransceive(id, SEL, NSData *)
{
    if (!globalNfcService)
        return nil;
    return globalNfcService->transceive();
}

} // namespace

#endif // HAVE(NEAR_FIELD)

MockNfcService::MockNfcService(Observer& observer, const MockWebAuthenticationConfiguration& configuration)
    : NfcService(observer)
    , m_configuration(configuration)
{
}

NSData* MockNfcService::transceive()
{
    if (m_configuration.nfc->payloadBase64.isEmpty())
        return nil;

    auto result = [[NSData alloc] initWithBase64EncodedString:m_configuration.nfc->payloadBase64[0] options:NSDataBase64DecodingIgnoreUnknownCharacters];
    m_configuration.nfc->payloadBase64.remove(0);
    return [result autorelease];
}

void MockNfcService::platformStartDiscovery()
{
#if HAVE(NEAR_FIELD)
    if (!!m_configuration.nfc) {
        globalNfcService = this;

        Method methodToSwizzle1 = class_getInstanceMethod(getNFReaderSessionClass(), @selector(setDelegate:));
        method_setImplementation(methodToSwizzle1, (IMP)NFReaderSessionSetDelegate);

        Method methodToSwizzle2 = class_getInstanceMethod(getNFReaderSessionClass(), @selector(connectTag:));
        if (m_configuration.nfc->error == MockNfc::Error::NoConnections)
            method_setImplementation(methodToSwizzle2, (IMP)NFReaderSessionConnectTagFail);
        else
            method_setImplementation(methodToSwizzle2, (IMP)NFReaderSessionConnectTag);

        Method methodToSwizzle3 = class_getInstanceMethod(getNFReaderSessionClass(), @selector(transceive:));
        method_setImplementation(methodToSwizzle3, (IMP)NFReaderSessionTransceive);

        auto readerSession = adoptNS([allocNFReaderSessionInstance() init]);
        setDriver(WTF::makeUnique<CtapNfcDriver>(makeUniqueRef<NfcConnection>(readerSession.get(), *this)));

        RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)] {
            if (!weakThis)
                return;
            weakThis->detectTags();
        });
        return;
    }
    LOG_ERROR("No nfc authenticators is available.");
#endif // HAVE(NEAR_FIELD)
}

void MockNfcService::detectTags() const
{
#if HAVE(NEAR_FIELD)
    if (m_configuration.nfc->error == MockNfc::Error::NoTags)
        return;

    auto callback = makeBlockPtr([configuration = m_configuration] {
        auto tags = adoptNS([[NSMutableArray alloc] init]);
        if (configuration.nfc->error == MockNfc::Error::WrongTagType || configuration.nfc->multipleTags)
            [tags addObject:[[WKMockNFTag alloc] initWithType:NFTagTypeUnknown]];
        else
            [tags addObject:[[WKMockNFTag alloc] initWithType:NFTagTypeGeneric4A]];

        if (configuration.nfc->multipleTags)
            [tags addObject:[[WKMockNFTag alloc] initWithType:NFTagTypeGeneric4A]];

        [globalNFReaderSessionDelegate readerSession:nil didDetectTags:tags.get()];
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), callback.get());
#endif // HAVE(NEAR_FIELD)
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
