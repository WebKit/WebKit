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
#import <objc/runtime.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

#import "NearFieldSoftLink.h"

namespace {
uint8_t tagID1[] = { 0x01 };
uint8_t tagID2[] = { 0x02 };
}

#if HAVE(NEAR_FIELD)

@interface WKMockNFTag : NSObject <NFTag>

- (instancetype)initWithType:(NFTagType)type;
- (instancetype)initWithType:(NFTagType)type tagID:(NSData *)tagID;

@end

@implementation WKMockNFTag {
    NFTagType _type;
    RetainPtr<NSData> _tagID;
}

@synthesize technology = _technology;
@synthesize AppData = _AppData;
@synthesize UID = _UID;
@synthesize ndefAvailability = _ndefAvailability;
@synthesize ndefMessageSize = _ndefMessageSize;
@synthesize ndefContainerSize = _ndefContainerSize;
@synthesize tagA = _tagA;
@synthesize tagB = _tagB;
@synthesize tagF = _tagF;

- (NFTagType)type
{
    return _type;
}

- (NSData *)tagID
{
    return _tagID.get();
}

- (instancetype)initWithNFTag:(id<NFTag>)tag
{
    if ((self = [super init])) {
        _type = tag.type;
        _tagID = tag.tagID;
    }
    return self;
}

- (void)dealloc
{
    SUPPRESS_UNRETAINED_ARG [_AppData release];
    _AppData = nil;
    SUPPRESS_UNRETAINED_ARG [_UID release];
    _UID = nil;

    [super dealloc];
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
    return [self initWithType:type tagID:toNSDataNoCopy(std::span { tagID1 }, FreeWhenDone::No).get()];
}

- (instancetype)initWithType:(NFTagType)type tagID:(NSData *)tagID
{
    if ((self = [super init])) {
        _type = type;
        _tagID = tagID;
    }
    return self;
}

@end

#endif // HAVE(NEAR_FIELD)

namespace WebKit {
using namespace fido;
using Mock = WebCore::MockWebAuthenticationConfiguration;

#if HAVE(NEAR_FIELD)

namespace {

static id<NFReaderSessionDelegate> globalNFReaderSessionDelegate;

static WeakPtr<MockNfcService>& NODELETE weakGlobalNfcService()
{
    static NeverDestroyed<WeakPtr<MockNfcService>> service;
    return service.get();
}

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

static BOOL NFReaderSessionStopPolling(id, SEL)
{
    RefPtr globalNfcService = weakGlobalNfcService().get();
    if (!globalNfcService)
        return NO;
    globalNfcService->receiveStopPolling();
    return YES;
}

static BOOL NFReaderSessionStartPollingWithError(id, SEL, NSError **)
{
    RefPtr globalNfcService = weakGlobalNfcService().get();
    if (!globalNfcService)
        return NO;
    globalNfcService->receiveStartPolling();
    return YES;
}

static NSData* NFReaderSessionTransceive(id, SEL, NSData *)
{
    RefPtr globalNfcService = weakGlobalNfcService().get();
    return globalNfcService ? globalNfcService->transceive() : nil;
}

static void NFSessionEndSessionWithCompletion(id, SEL, void (^completion)(void)) {
    if (completion)
        completion();
}

static id NFHardwareManagerSharedManager(id, SEL)
{
    static NeverDestroyed<RetainPtr<id>> manager = adoptNS([[getNFHardwareManagerClassSingleton() alloc] init]);
    return manager.get().get();
}

static BOOL NFHardwareManagerAreFeaturesSupported(id, SEL, NFFeature, NSError**)
{
    return YES;
}

static void NFHardwareManagerStartReaderSessionWithBusyError(id, SEL, void (^callback)(NFReaderSession*, NSError*)) {
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSError *error = [NSError errorWithDomain:@"com.apple.nfcd" code:2 userInfo:@{ NSLocalizedDescriptionKey: @"Busy" }];
        callback(nil, error);
    });
}

static IMP originalSharedHardwareManager;
static IMP originalAreFeaturesSupported;
static IMP originalStartReaderSession;

} // namespace

#endif // HAVE(NEAR_FIELD)

Ref<MockNfcService> MockNfcService::create(AuthenticatorTransportServiceObserver& observer, const WebCore::MockWebAuthenticationConfiguration& configuration)
{
    return adoptRef(*new MockNfcService(observer, configuration));
}

MockNfcService::MockNfcService(AuthenticatorTransportServiceObserver& observer, const WebCore::MockWebAuthenticationConfiguration& configuration)
    : NfcService(observer)
    , m_configuration(configuration)
{
}

NSData* MockNfcService::transceive()
{
    if (m_configuration.nfc->payloadBase64.isEmpty())
        return nil;

    auto result = adoptNS([[NSData alloc] initWithBase64EncodedString:m_configuration.nfc->payloadBase64[0].createNSString().get() options:NSDataBase64DecodingIgnoreUnknownCharacters]);
    m_configuration.nfc->payloadBase64.removeAt(0);
    return result.autorelease();
}

void MockNfcService::receiveStopPolling()
{
    // For purpose of restart polling.
    m_configuration.nfc->multiplePhysicalTags = false;
}

void MockNfcService::receiveStartPolling()
{
    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->detectTags();
    });
}

void MockNfcService::platformStartDiscovery()
{
#if HAVE(NEAR_FIELD)
    if (m_configuration.nfc && m_configuration.nfc->error == Mock::NfcError::HardwareBusy) {
        // Clear error so retry (via m_restartTimer) will succeed through normal mock path.
        m_configuration.nfc->error = Mock::NfcError::Success;

        // Ensure NFHardwareManager class exists. On platforms where the NearField
        // framework is not available (e.g., macOS), the soft-link returns nil. Register
        // a fake class so the soft-link accessor finds it on first call and the
        // subsequent method swizzles have a real class to operate on.
        if (!objc_getClass("NFHardwareManager")) {
            SUPPRESS_UNRETAINED_LOCAL Class cls = objc_allocateClassPair([NSObject class], "NFHardwareManager", 0);
            ASSERT(cls);
            SUPPRESS_UNRETAINED_LOCAL Class metaCls = object_getClass(cls);
            class_addMethod(metaCls, @selector(sharedHardwareManager), (IMP)NFHardwareManagerSharedManager, "@@:");
            class_addMethod(cls, @selector(areFeaturesSupported:outError:), (IMP)NFHardwareManagerAreFeaturesSupported, "B@:I^@");
            class_addMethod(cls, @selector(startReaderSession:), (IMP)NFHardwareManagerStartReaderSessionWithBusyError, "@@:@?");
            objc_registerClassPair(cls);
        }

        Method hmMethod0 = class_getClassMethod(getNFHardwareManagerClassSingleton(), @selector(sharedHardwareManager));
        originalSharedHardwareManager = method_setImplementation(hmMethod0, (IMP)NFHardwareManagerSharedManager);

        Method hmMethod1 = class_getInstanceMethod(getNFHardwareManagerClassSingleton(), @selector(areFeaturesSupported:outError:));
        originalAreFeaturesSupported = method_setImplementation(hmMethod1, (IMP)NFHardwareManagerAreFeaturesSupported);

        Method hmMethod2 = class_getInstanceMethod(getNFHardwareManagerClassSingleton(), @selector(startReaderSession:));
        originalStartReaderSession = method_setImplementation(hmMethod2, (IMP)NFHardwareManagerStartReaderSessionWithBusyError);

        NfcService::platformStartDiscovery();
        return;
    }
    if (!!m_configuration.nfc) {
        // Restore any swizzled NFHardwareManager methods from the HardwareBusy path.
        if (originalSharedHardwareManager) {
            method_setImplementation(class_getClassMethod(getNFHardwareManagerClassSingleton(), @selector(sharedHardwareManager)), originalSharedHardwareManager);
            originalSharedHardwareManager = nil;
        }
        if (originalAreFeaturesSupported) {
            method_setImplementation(class_getInstanceMethod(getNFHardwareManagerClassSingleton(), @selector(areFeaturesSupported:outError:)), originalAreFeaturesSupported);
            originalAreFeaturesSupported = nil;
        }
        if (originalStartReaderSession) {
            method_setImplementation(class_getInstanceMethod(getNFHardwareManagerClassSingleton(), @selector(startReaderSession:)), originalStartReaderSession);
            originalStartReaderSession = nil;
        }

        weakGlobalNfcService() = *this;

        Method methodToSwizzle1 = class_getInstanceMethod(getNFReaderSessionClassSingleton(), @selector(setDelegate:));
        method_setImplementation(methodToSwizzle1, (IMP)NFReaderSessionSetDelegate);

        Method methodToSwizzle2 = class_getInstanceMethod(getNFReaderSessionClassSingleton(), @selector(connectTag:));
        if (m_configuration.nfc->error == Mock::NfcError::NoConnections)
            method_setImplementation(methodToSwizzle2, (IMP)NFReaderSessionConnectTagFail);
        else
            method_setImplementation(methodToSwizzle2, (IMP)NFReaderSessionConnectTag);

        Method methodToSwizzle3 = class_getInstanceMethod(getNFReaderSessionClassSingleton(), @selector(transceive:));
        method_setImplementation(methodToSwizzle3, (IMP)NFReaderSessionTransceive);

        Method methodToSwizzle4 = class_getInstanceMethod(getNFReaderSessionClassSingleton(), @selector(stopPolling));
        method_setImplementation(methodToSwizzle4, (IMP)NFReaderSessionStopPolling);

        Method methodToSwizzle5 = class_getInstanceMethod(getNFReaderSessionClassSingleton(), @selector(startPollingWithError:));
        method_setImplementation(methodToSwizzle5, (IMP)NFReaderSessionStartPollingWithError);

        // endSessionWithCompletion: is declared on NFSession (superclass), not NFReaderSession.
        // Use class_replaceMethod to add it directly to NFReaderSession so the mock completion
        // handler fires during restartDiscoveryInternal without modifying the superclass.
        class_replaceMethod(getNFReaderSessionClassSingleton(), @selector(endSessionWithCompletion:), (IMP)NFSessionEndSessionWithCompletion, "v@:@?");

        auto readerSession = adoptNS([allocNFReaderSessionInstance() initWithUIType:NFReaderSessionUINone]);
        setConnection(NfcConnection::create(readerSession.get(), *this));
    }
    LOG_ERROR("No nfc authenticators is available.");
#endif // HAVE(NEAR_FIELD)
}

void MockNfcService::detectTags() const
{
#if HAVE(NEAR_FIELD)
    if (m_configuration.nfc->error == Mock::NfcError::NoTags)
        return;

    auto callback = makeBlockPtr([configuration = m_configuration] {
        auto tags = adoptNS([[NSMutableArray alloc] init]);
        if (configuration.nfc->error == Mock::NfcError::WrongTagType || configuration.nfc->multipleTags)
            [tags addObject:adoptNS([[WKMockNFTag alloc] initWithType:NFTagTypeUnknown]).get()];
        else
            [tags addObject:adoptNS([[WKMockNFTag alloc] initWithType:NFTagTypeGeneric4A]).get()];

        if (configuration.nfc->multipleTags)
            [tags addObject:adoptNS([[WKMockNFTag alloc] initWithType:NFTagTypeGeneric4A]).get()];

        if (configuration.nfc->multiplePhysicalTags)
            [tags addObject:adoptNS([[WKMockNFTag alloc] initWithType:NFTagTypeGeneric4A tagID:toNSDataNoCopy(std::span { tagID2 }, FreeWhenDone::No).get()]).get()];

        auto readerSession = adoptNS([allocNFReaderSessionInstance() initWithUIType:NFReaderSessionUINone]);
        [protect(globalNFReaderSessionDelegate) readerSession:readerSession.get() didDetectTags:tags.get()];
    });
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), callback.get());
#endif // HAVE(NEAR_FIELD)
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
