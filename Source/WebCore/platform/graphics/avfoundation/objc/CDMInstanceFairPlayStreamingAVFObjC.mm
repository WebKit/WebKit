/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "CDMInstanceFairPlayStreamingAVFObjC.h"

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

#import "CDMFairPlayStreaming.h"
#import "CDMKeySystemConfiguration.h"
#import "CDMMediaCapability.h"
#import "InitDataRegistry.h"
#import "Logging.h"
#import "NotImplemented.h"
#import "SharedBuffer.h"
#import "TextDecoder.h"
#import <AVFoundation/AVContentKeySession.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Algorithms.h>
#import <wtf/FileSystem.h>
#import <wtf/JSONValues.h>
#import <wtf/LoggerHelper.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringHash.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

static NSString * const InitializationDataTypeKey = @"InitializationDataType";
static NSString * const ContentKeyReportGroupKey = @"ContentKeyReportGroup";
static const NSInteger SecurityLevelError = -42811;

@interface WebCoreFPSContentKeySessionDelegate : NSObject<AVContentKeySessionDelegate> {
    WebCore::AVContentKeySessionDelegateClient* _parent;
}
@end

@implementation WebCoreFPSContentKeySessionDelegate
- (id)initWithParent:(WebCore::AVContentKeySessionDelegateClient *)parent
{
    if (!(self = [super init]))
        return nil;

    _parent = parent;
    return self;
}

- (void)invalidate
{
    _parent = nil;
}

- (void)contentKeySession:(AVContentKeySession *)session didProvideContentKeyRequest:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didProvideRequest(keyRequest);
}

- (void)contentKeySession:(AVContentKeySession *)session didProvideRenewingContentKeyRequest:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didProvideRenewingRequest(keyRequest);
}

#if PLATFORM(IOS_FAMILY)
- (void)contentKeySession:(AVContentKeySession *)session didProvidePersistableContentKeyRequest:(AVPersistableContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didProvidePersistableRequest(keyRequest);
}

- (void)contentKeySession:(AVContentKeySession *)session didUpdatePersistableContentKey:(NSData *)persistableContentKey forContentKeyIdentifier:(id)keyIdentifier
{
    UNUSED_PARAM(session);
    UNUSED_PARAM(persistableContentKey);
    UNUSED_PARAM(keyIdentifier);
    notImplemented();
}
#endif

- (void)contentKeySession:(AVContentKeySession *)session didProvideContentKeyRequests:(NSArray<AVContentKeyRequest *> *)keyRequests forInitializationData:(nullable NSData *)initializationData
{
    UNUSED_PARAM(session);
    UNUSED_PARAM(initializationData);
    if (!_parent)
        return;

    Vector<RetainPtr<AVContentKeyRequest>> requests;
    requests.reserveInitialCapacity(keyRequests.count);
    [keyRequests enumerateObjectsUsingBlock:[&](AVContentKeyRequest* request, NSUInteger, BOOL*) {
        requests.uncheckedAppend(request);
    }];
    _parent->didProvideRequests(WTFMove(requests));
}

- (void)contentKeySession:(AVContentKeySession *)session contentKeyRequest:(AVContentKeyRequest *)keyRequest didFailWithError:(NSError *)err
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->didFailToProvideRequest(keyRequest, err);
}

- (void)contentKeySession:(AVContentKeySession *)session contentKeyRequestDidSucceed:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->requestDidSucceed(keyRequest);
}

- (BOOL)contentKeySession:(AVContentKeySession *)session shouldRetryContentKeyRequest:(AVContentKeyRequest *)keyRequest reason:(AVContentKeyRequestRetryReason)retryReason
{
    UNUSED_PARAM(session);
    return _parent ? _parent->shouldRetryRequestForReason(keyRequest, retryReason) : false;
}

- (void)contentKeySessionContentProtectionSessionIdentifierDidChange:(AVContentKeySession *)session
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->sessionIdentifierChanged(session.contentProtectionSessionIdentifier);
}

- (void)contentKeySession:(AVContentKeySession *)session contentProtectionSessionIdentifierDidChangeForKeyGroup:(nullable AVContentKeyReportGroup *)reportGroup
{
    // FIXME: Remove after rdar://57430747 is fixed
    [self contentKeySession:session contentProtectionSessionIdentifierDidChangeForReportGroup:reportGroup];
}

- (void)contentKeySession:(AVContentKeySession *)session contentProtectionSessionIdentifierDidChangeForReportGroup:(nullable AVContentKeyReportGroup *)reportGroup
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->groupSessionIdentifierChanged(reportGroup, reportGroup.contentProtectionSessionIdentifier);
}

- (void)contentKeySession:(AVContentKeySession *)session externalProtectionStatusDidChangeForContentKeyRequest:(AVContentKeyRequest *)keyRequest
{
    UNUSED_PARAM(session);
    if (_parent)
        _parent->externalProtectionStatusDidChangeForContentKeyRequest(keyRequest);
}

@end

namespace WTF {

template<typename>
struct LogArgument;

template<>
struct LogArgument<WebCore::CDMInstanceFairPlayStreamingAVFObjC::Keys> {
    static String toString(const WebCore::CDMInstanceFairPlayStreamingAVFObjC::Keys& keys)
    {
        StringBuilder builder;
        builder.append("[");
        for (auto key : keys)
            builder.append(key->toHexString());
        builder.append("]");
        return builder.toString();
    }
};

}

namespace WebCore {

#if !RELEASE_LOG_DISABLED
static WTFLogChannel& logChannel() { return LogEME; }
#endif

static AtomString initTypeForRequest(AVContentKeyRequest* request)
{
    if ([request.identifier isKindOfClass:NSString.class] && [request.identifier hasPrefix:@"skd://"])
        return CDMPrivateFairPlayStreaming::skdName();

    if (![request respondsToSelector:@selector(options)]) {
        // AVContentKeyRequest.options was added in 10.14.4; if we are running on a previous version
        // we don't have support for 'cenc' anyway, so just assume 'sinf'.
        return CDMPrivateFairPlayStreaming::sinfName();
    }

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto nsInitType = (NSString*)[request.options valueForKey:InitializationDataTypeKey];
ALLOW_NEW_API_WITHOUT_GUARDS_END
    if (![nsInitType isKindOfClass:NSString.class]) {
        // The only way for an initialization data to end up here without an appropriate key in
        // the options dictionary is for that data to have been generated by the AVStreamDataParser
        // and currently, the only initialization data supported by the parser is the 'sinf' kind.
        return CDMPrivateFairPlayStreaming::sinfName();
    }

    return AtomString(nsInitType);
}

static Ref<SharedBuffer> initializationDataForRequest(AVContentKeyRequest* request)
{
    if (!request)
        return SharedBuffer::create();

    if (initTypeForRequest(request) == CDMPrivateFairPlayStreaming::skdName())
        return SharedBuffer::create([request.identifier dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES]);

    return SharedBuffer::create(request.initializationData);
}

CDMInstanceFairPlayStreamingAVFObjC::CDMInstanceFairPlayStreamingAVFObjC() = default;

#if !RELEASE_LOG_DISABLED
void CDMInstanceFairPlayStreamingAVFObjC::setLogger(Logger& logger, const void* logIdentifier)
{
    m_logger = &logger;
    m_logIdentifier = logIdentifier;
}
#endif

AVContentKeySession* CDMInstanceFairPlayStreamingAVFObjC::contentKeySession()
{
    if (m_session)
        return m_session.get();

    if (!PAL::canLoad_AVFoundation_AVContentKeySystemFairPlayStreaming())
        return nullptr;

    if (!PAL::getAVContentKeyReportGroupClass())
        return nullptr;

    auto storageURL = this->storageURL();
    if (!persistentStateAllowed() || !storageURL)
        m_session = [PAL::getAVContentKeySessionClass() contentKeySessionWithKeySystem:AVContentKeySystemFairPlayStreaming];
    else
        m_session = [PAL::getAVContentKeySessionClass() contentKeySessionWithKeySystem:AVContentKeySystemFairPlayStreaming storageDirectoryAtURL:storageURL];

    if (!m_session)
        return nullptr;

    if (!m_delegate)
        m_delegate = adoptNS([[WebCoreFPSContentKeySessionDelegate alloc] initWithParent:this]);

    [m_session setDelegate:m_delegate.get() queue:dispatch_get_main_queue()];
    return m_session.get();
}

RetainPtr<AVContentKeyRequest> CDMInstanceFairPlayStreamingAVFObjC::takeUnexpectedKeyRequestForInitializationData(const AtomString& initDataType, SharedBuffer& initData)
{
    for (auto requestIter = m_unexpectedKeyRequests.begin(); requestIter != m_unexpectedKeyRequests.end(); ++requestIter) {
        auto& request = *requestIter;
        auto requestType = initTypeForRequest(request.get());
        auto requestInitData = initializationDataForRequest(request.get());
        if (initDataType != requestType || initData != requestInitData.get())
            continue;

        return m_unexpectedKeyRequests.take(requestIter);
    }
    return nullptr;
}

class CDMInstanceSessionFairPlayStreamingAVFObjC::UpdateResponseCollector {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using KeyType = RetainPtr<AVContentKeyRequest>;
    using ValueType = RetainPtr<NSData>;
    using ResponseMap = HashMap<KeyType, ValueType>;
    using UpdateCallback = WTF::Function<void(std::optional<ResponseMap>&&)>;
    UpdateResponseCollector(size_t numberOfExpectedResponses, UpdateCallback&& callback)
        : m_numberOfExpectedResponses(numberOfExpectedResponses)
        , m_callback(WTFMove(callback))
    {
    }

    void addSuccessfulResponse(AVContentKeyRequest* request, NSData* data)
    {
        m_responses.set(request, data);
        if (!--m_numberOfExpectedResponses)
            m_callback(WTFMove(m_responses));
    }
    void addErrorResponse(AVContentKeyRequest* request, NSError*)
    {
        m_responses.set(request, nullptr);
        if (!--m_numberOfExpectedResponses)
            m_callback(WTFMove(m_responses));
    }
    void fail()
    {
        m_callback(std::nullopt);
    }

private:
    size_t m_numberOfExpectedResponses;
    UpdateCallback m_callback;
    ResponseMap m_responses;
};

static RefPtr<JSON::Value> parseJSONValue(const SharedBuffer& buffer)
{
    // Fail on large buffers whose size doesn't fit into a 32-bit unsigned integer.
    size_t size = buffer.size();
    if (size > std::numeric_limits<unsigned>::max())
        return nullptr;

    // Parse the buffer contents as JSON, returning the root object (if any).
    String json { buffer.makeContiguous()->data(), static_cast<unsigned>(size) };

    return JSON::Value::parseJSON(json);
}

bool CDMInstanceFairPlayStreamingAVFObjC::supportsPersistableState()
{
    return [PAL::getAVContentKeySessionClass() respondsToSelector:@selector(pendingExpiredSessionReportsWithAppIdentifier:storageDirectoryAtURL:)];
}

bool CDMInstanceFairPlayStreamingAVFObjC::supportsPersistentKeys()
{
#if PLATFORM(IOS_FAMILY)
    return PAL::getAVPersistableContentKeyRequestClass();
#else
    return false;
#endif
}

bool CDMInstanceFairPlayStreamingAVFObjC::supportsMediaCapability(const CDMMediaCapability& capability)
{
    if (![PAL::getAVURLAssetClass() isPlayableExtendedMIMEType:capability.contentType])
        return false;

    // FairPlay only supports 'cbcs' encryption:
    if (capability.encryptionScheme && capability.encryptionScheme.value() != CDMEncryptionScheme::cbcs)
        return false;

    return true;
}

void CDMInstanceFairPlayStreamingAVFObjC::initializeWithConfiguration(const CDMKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers, AllowPersistentState persistentState, SuccessCallback&& callback)
{
    // FIXME: verify that FairPlayStreaming does not (and cannot) expose a distinctive identifier to the client
    auto initialize = [&] () {
        if (configuration.distinctiveIdentifier == CDMRequirement::Required)
            return Failed;

        if (configuration.persistentState != CDMRequirement::Required && (configuration.sessionTypes.contains(CDMSessionType::PersistentUsageRecord) || configuration.sessionTypes.contains(CDMSessionType::PersistentLicense)))
            return Failed;

        if (configuration.persistentState == CDMRequirement::Required && !m_storageURL)
            return Failed;

        if (configuration.sessionTypes.contains(CDMSessionType::PersistentLicense) && !supportsPersistentKeys())
            return Failed;

        if (!PAL::canLoad_AVFoundation_AVContentKeySystemFairPlayStreaming())
            return Failed;

        m_persistentStateAllowed = persistentState == AllowPersistentState::Yes;
        return Succeeded;
    };
    callback(initialize());
}

void CDMInstanceFairPlayStreamingAVFObjC::setServerCertificate(Ref<SharedBuffer>&& serverCertificate, SuccessCallback&& callback)
{
    DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_serverCertificate = WTFMove(serverCertificate);
    callback(Succeeded);
}

void CDMInstanceFairPlayStreamingAVFObjC::setStorageDirectory(const String& storageDirectory)
{
    DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (storageDirectory.isEmpty()) {
        m_storageURL = nil;
        return;
    }

    auto storagePath = FileSystem::pathByAppendingComponent(storageDirectory, "SecureStop.plist");

    auto fileType = FileSystem::fileTypeFollowingSymlinks(storageDirectory);
    if (!fileType) {
        if (!FileSystem::makeAllDirectories(storageDirectory))
            return;
    } else if (*fileType != FileSystem::FileType::Directory) {
        auto tempDirectory = FileSystem::createTemporaryDirectory(@"MediaKeys");
        if (!tempDirectory)
            return;

        auto tempStoragePath = FileSystem::pathByAppendingComponent(tempDirectory, FileSystem::pathFileName(storagePath));
        if (!FileSystem::moveFile(storageDirectory, tempStoragePath))
            return;

        if (!FileSystem::moveFile(tempDirectory, storageDirectory))
            return;
    }

    m_storageURL = adoptNS([[NSURL alloc] initFileURLWithPath:storagePath isDirectory:NO]);
}

RefPtr<CDMInstanceSession> CDMInstanceFairPlayStreamingAVFObjC::createSession()
{
    auto session = adoptRef(*new CDMInstanceSessionFairPlayStreamingAVFObjC(*this));
    m_sessions.append(session);
    return session;
}

void CDMInstanceFairPlayStreamingAVFObjC::setClient(WeakPtr<CDMInstanceClient>&& client)
{
    m_client = WTFMove(client);
}

void CDMInstanceFairPlayStreamingAVFObjC::clearClient()
{
    m_client = nullptr;
}

const String& CDMInstanceFairPlayStreamingAVFObjC::keySystem() const
{
    static NeverDestroyed<String> keySystem { "com.apple.fps"_s };
    return keySystem;
}

static AVContentKeyReportGroup* groupForRequest(AVContentKeyRequest* request)
{
    ASSERT([request respondsToSelector:@selector(options)]);
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto group = (AVContentKeyReportGroup*)[request.options valueForKey:ContentKeyReportGroupKey];
    ALLOW_NEW_API_WITHOUT_GUARDS_END
    return group;
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRequest(AVContentKeyRequest *request)
{
    if (auto* session = sessionForRequest(request)) {
        session->didProvideRequest(request);
        return;
    }

    DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- Unexpected request");

    handleUnexpectedRequests({{ request }});
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRequests(Vector<RetainPtr<AVContentKeyRequest>>&& requests)
{
    if (auto* session = sessionForRequest(requests.first().get())) {
        session->didProvideRequests(WTFMove(requests));
        return;
    }

    handleUnexpectedRequests(WTFMove(requests));
}

void CDMInstanceFairPlayStreamingAVFObjC::handleUnexpectedRequests(Vector<RetainPtr<AVContentKeyRequest>>&& requests)
{
    while (!requests.isEmpty()) {
        auto request = requests.takeLast();
        m_unexpectedKeyRequests.add(request);

        if (m_client)
            m_client->unrequestedInitializationDataReceived(initTypeForRequest(request.get()), initializationDataForRequest(request.get()));
    }
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvideRenewingRequest(AVContentKeyRequest *request)
{
    if (auto* session = sessionForRequest(request)) {
        session->didProvideRenewingRequest(request);
        return;
    }

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
}

void CDMInstanceFairPlayStreamingAVFObjC::didProvidePersistableRequest(AVContentKeyRequest *request)
{
    if (auto* session = sessionForRequest(request)) {
        session->didProvidePersistableRequest(request);
        return;
    }

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
}

void CDMInstanceFairPlayStreamingAVFObjC::didFailToProvideRequest(AVContentKeyRequest *request, NSError *error)
{
    if (auto* session = sessionForRequest(request)) {
        session->didFailToProvideRequest(request, error);
        return;
    }

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
}

void CDMInstanceFairPlayStreamingAVFObjC::requestDidSucceed(AVContentKeyRequest *request)
{
    if (auto* session = sessionForRequest(request)) {
        session->requestDidSucceed(request);
        return;
    }

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
}

bool CDMInstanceFairPlayStreamingAVFObjC::shouldRetryRequestForReason(AVContentKeyRequest *request, NSString *reason)
{
    if (auto* session = sessionForRequest(request))
        return session->shouldRetryRequestForReason(request, reason);

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
    return false;
}

void CDMInstanceFairPlayStreamingAVFObjC::sessionIdentifierChanged(NSData *)
{
    // No-op. If we are getting this callback, there are outstanding AVContentKeyRequestGroups which are managing their own
    // session identifiers.
}

void CDMInstanceFairPlayStreamingAVFObjC::groupSessionIdentifierChanged(AVContentKeyReportGroup* group, NSData *sessionIdentifier)
{
    if (group == [m_session defaultContentKeyGroup]) {
        INFO_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- default unused group identifier changed; dropping");
        return;
    }

    if (auto* session = sessionForGroup(group)) {
        session->groupSessionIdentifierChanged(group, sessionIdentifier);
        return;
    }

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
}

void CDMInstanceFairPlayStreamingAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
    for (auto& sessionInterface : m_sessions) {
        if (sessionInterface)
            sessionInterface->outputObscuredDueToInsufficientExternalProtectionChanged(obscured);
    }
}

void CDMInstanceFairPlayStreamingAVFObjC::externalProtectionStatusDidChangeForContentKeyRequest(AVContentKeyRequest* request)
{
    if (auto* session = sessionForRequest(request)) {
        session->externalProtectionStatusDidChangeForContentKeyRequest(request);
        return;
    }

    ASSERT_NOT_REACHED();
    ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "- no responsible session; dropping");
}

CDMInstanceSessionFairPlayStreamingAVFObjC* CDMInstanceFairPlayStreamingAVFObjC::sessionForKeyIDs(const Keys& keyIDs) const
{
    for (auto& sessionInterface : m_sessions) {
        if (!sessionInterface)
            continue;

        auto sessionKeys = sessionInterface->keyIDs();
        if (anyOf(sessionKeys, [&](const Ref<FragmentedSharedBuffer>& sessionKey) {
            return keyIDs.findMatching([&](const Ref<FragmentedSharedBuffer>& keyID) {
                return keyID.get() == sessionKey.get();
            }) != notFound;
        }))
        return sessionInterface.get();
    }
    return nullptr;
}

CDMInstanceSessionFairPlayStreamingAVFObjC* CDMInstanceFairPlayStreamingAVFObjC::sessionForRequest(AVContentKeyRequest* request) const
{
    auto index = m_sessions.findMatching([&] (auto session) {
        return session && session->hasRequest(request);
    });

    if (index != notFound)
        return m_sessions[index].get();

    return sessionForGroup(groupForRequest(request));
}

CDMInstanceSessionFairPlayStreamingAVFObjC* CDMInstanceFairPlayStreamingAVFObjC::sessionForGroup(AVContentKeyReportGroup* group) const
{
    auto index = m_sessions.findMatching([group] (auto session) {
        return session && session->contentKeyReportGroup() == group;
    });

    if (index != notFound)
        return m_sessions[index].get();
    return nullptr;
}

CDMInstanceSessionFairPlayStreamingAVFObjC::CDMInstanceSessionFairPlayStreamingAVFObjC(Ref<CDMInstanceFairPlayStreamingAVFObjC>&& instance)
    : m_instance(WTFMove(instance))
    , m_delegate(adoptNS([[WebCoreFPSContentKeySessionDelegate alloc] initWithParent:this]))
{
}

CDMInstanceSessionFairPlayStreamingAVFObjC::~CDMInstanceSessionFairPlayStreamingAVFObjC()
{
    [m_delegate invalidate];
}

#if !RELEASE_LOG_DISABLED
void CDMInstanceSessionFairPlayStreamingAVFObjC::setLogger(Logger& logger, const void* logIdentifier)
{
    m_logger = &logger;
    m_logIdentifier = logIdentifier;
}
#endif

using Keys = CDMInstanceSessionFairPlayStreamingAVFObjC::Keys;
static Keys keyIDsForRequest(AVContentKeyRequest* request)
{
    if ([request.identifier isKindOfClass:[NSString class]])
        return Keys::from(SharedBuffer::create([(NSString *)request.identifier dataUsingEncoding:NSUTF8StringEncoding]));
    if ([request.identifier isKindOfClass:[NSData class]])
        return Keys::from(SharedBuffer::create((NSData *)request.identifier));
    if (request.initializationData) {
        if (auto sinfKeyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(SharedBuffer::create(request.initializationData)))
            return WTFMove(sinfKeyIDs.value());
    }
    return { };
}

using Request = CDMInstanceSessionFairPlayStreamingAVFObjC::Request;
static Keys keyIDsForRequest(const Request& requests)
{
    Keys keyIDs;
    for (auto& request : requests.requests)
        keyIDs.appendVector(keyIDsForRequest(request.get()));
    return keyIDs;
}

Keys CDMInstanceSessionFairPlayStreamingAVFObjC::keyIDs()
{
    // FIXME(rdar://problem/35597141): use the future AVContentKeyRequest keyID property, rather than parsing it out of the init
    // data, to get the keyID.
    Keys keyIDs;
    for (auto& request : m_requests) {
        for (auto& key : keyIDsForRequest(request))
            keyIDs.append(WTFMove(key));
    }

    return keyIDs;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::requestLicense(LicenseType licenseType, const AtomString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    if (!isLicenseTypeSupported(licenseType)) {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " false, licenseType \"", licenseType, "\" not supported");
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    if (!m_instance->serverCertificate()) {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " false, no serverCertificate");
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    if (!ensureSessionOrGroup()) {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " false, could not create session or group object");
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    if (auto unexpectedRequest = m_instance->takeUnexpectedKeyRequestForInitializationData(initDataType, initData)) {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " found unexpectedRequest matching initData");
        if (m_group) {
            ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
            [m_group associateContentKeyRequest:unexpectedRequest.get()];
            ALLOW_NEW_API_WITHOUT_GUARDS_END
        }
        m_requestLicenseCallback = WTFMove(callback);
        didProvideRequest(unexpectedRequest.get());
        return;
    }

    RetainPtr<NSString> identifier;
    RetainPtr<NSData> initializationData;

    if (initDataType == CDMPrivateFairPlayStreaming::sinfName())
        initializationData = initData->makeContiguous()->createNSData();
    else if (initDataType == CDMPrivateFairPlayStreaming::skdName())
        identifier = adoptNS([[NSString alloc] initWithData:initData->makeContiguous()->createNSData().get() encoding:NSUTF8StringEncoding]);
#if HAVE(FAIRPLAYSTREAMING_CENC_INITDATA)
    else if (initDataType == InitDataRegistry::cencName()) {
        auto psshString = base64EncodeToString(initData->makeContiguous()->data(), initData->size());
        initializationData = [NSJSONSerialization dataWithJSONObject:@{ @"pssh": (NSString*)psshString } options:NSJSONWritingPrettyPrinted error:nil];
    }
#endif
    else {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " false, initDataType not suppported");
        callback(SharedBuffer::create(), emptyString(), false, Failed);
        return;
    }

    DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " processing request");
    m_requestLicenseCallback = WTFMove(callback);

    if (m_group) {
        auto* options = @{ ContentKeyReportGroupKey: m_group.get(), InitializationDataTypeKey: (NSString*)initDataType };
        [m_group processContentKeyRequestWithIdentifier:identifier.get() initializationData:initializationData.get() options:options];
    } else {
        auto* options = @{ InitializationDataTypeKey: (NSString*)initDataType };
        [m_session processContentKeyRequestWithIdentifier:identifier.get() initializationData:initializationData.get() options:options];
    }
}

static bool isEqual(const SharedBuffer& data, const String& value)
{
    auto arrayBuffer = data.tryCreateArrayBuffer();
    if (!arrayBuffer)
        return false;

    auto exceptionOrDecoder = TextDecoder::create("utf8"_s, TextDecoder::Options());
    if (exceptionOrDecoder.hasException())
        return false;

    Ref<TextDecoder> decoder = exceptionOrDecoder.releaseReturnValue();
    auto stringOrException = decoder->decode(BufferSource::VariantType(WTFMove(arrayBuffer)), TextDecoder::DecodeOptions());
    if (stringOrException.hasException())
        return false;

    return stringOrException.returnValue() == value;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::updateLicense(const String&, LicenseType, Ref<SharedBuffer>&& responseData, LicenseUpdateCallback&& callback)
{
    if (!m_expiredSessions.isEmpty() && isEqual(responseData, "acknowledged"_s)) {
        auto* certificate = m_instance->serverCertificate();
        auto* storageURL = m_instance->storageURL();

        if (!certificate || !storageURL) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "\"acknowledged\", Failed, no certificate and storageURL");
            callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
            return;
        }

        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "\"acknowledged\", Succeeded, removing expired session report");
        auto expiredSessions = createNSArray(m_expiredSessions, [] (auto& data) {
            return data.get();
        });
        auto appIdentifier = certificate->makeContiguous()->createNSData();
        [PAL::getAVContentKeySessionClass() removePendingExpiredSessionReports:expiredSessions.get() withAppIdentifier:appIdentifier.get() storageDirectoryAtURL:storageURL];
        callback(false, { }, std::nullopt, std::nullopt, Succeeded);
        return;
    }

    if (!m_requests.isEmpty() && isEqual(responseData, "renew"_s)) {
        auto request = lastKeyRequest();
        if (!request) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "\"renew\", Failed, no outstanding keys");
            callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
            return;
        }
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "\"renew\", processing renewal");
        auto session = m_session ? m_session.get() : m_instance->contentKeySession();
        [session renewExpiringResponseDataForContentKeyRequest:request];
        m_updateLicenseCallback = WTFMove(callback);
        return;
    }

    if (!m_currentRequest) {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, no currentRequest");
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }
    Keys keyIDs = keyIDsForRequest(m_currentRequest.value());
    if (keyIDs.isEmpty()) {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, no keyIDs in currentRequest");
        callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        return;
    }

    if (m_currentRequest.value().initType == InitDataRegistry::cencName()) {
        if (m_updateResponseCollector) {
            m_updateResponseCollector->fail();
            m_updateResponseCollector = nullptr;
        }

        m_updateResponseCollector = WTF::makeUnique<UpdateResponseCollector>(m_currentRequest.value().requests.size(), [weakThis = WeakPtr { *this }, this] (std::optional<UpdateResponseCollector::ResponseMap>&& responses) {
            if (!weakThis)
                return;

            if (!m_updateLicenseCallback)
                return;

            if (!responses || responses.value().isEmpty()) {
                DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "'cenc' initData, Failed, no responses");
                m_updateLicenseCallback(true, std::nullopt, std::nullopt, std::nullopt, Failed);
                return;
            }

            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "'cenc' initData, Succeeded, no keyIDs in currentRequest");
            m_updateLicenseCallback(false, keyStatuses(), std::nullopt, std::nullopt, Succeeded);
            m_updateResponseCollector = nullptr;
            m_currentRequest = std::nullopt;
            nextRequest();
        });

        auto root = parseJSONValue(responseData);
        if (!root) {
            callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
            return;
        }

        auto array = root->asArray();
        if (!array) {
            callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
            return;
        }

        auto parseResponse = [&](Ref<JSON::Value>&& value) -> bool {
            auto object = value->asObject();
            if (!object)
                return false;

            auto keyIDString = object->getString("keyID");
            if (!keyIDString)
                return false;

            auto keyIDVector = base64Decode(keyIDString);
            if (!keyIDVector)
                return false;

            auto keyID = SharedBuffer::create(WTFMove(*keyIDVector));
            auto foundIndex = m_currentRequest.value().requests.findMatching([&] (auto& request) {
                auto keyIDs = keyIDsForRequest(request.get());
                return keyIDs.findMatching([&](const Ref<FragmentedSharedBuffer>& id) {
                    return id.get() == keyID.get();
                }) != notFound;
            });
            if (foundIndex == notFound)
                return false;

            auto& request = m_currentRequest.value().requests[foundIndex];

            auto payloadFindResults = object->find("payload");
            auto errorFindResults = object->find("error");
            bool hasPayload = payloadFindResults != object->end();
            bool hasError = errorFindResults != object->end();

            // Either "payload" or "error" are present, but not both
            if (hasPayload == hasError)
                return false;

            if (hasError) {
                auto errorCode = errorFindResults->value->asInteger();
                if (!errorCode)
                    return false;
                auto error = adoptNS([[NSError alloc] initWithDomain:@"org.webkit.eme" code:*errorCode userInfo:nil]);
                [request processContentKeyResponseError:error.get()];
            } else if (hasPayload) {
                auto payloadString = payloadFindResults->value->asString();
                if (!payloadString)
                    return false;
                auto payloadVector = base64Decode(payloadString);
                if (!payloadVector)
                    return false;
                auto payloadData = SharedBuffer::create(WTFMove(*payloadVector));
                [request processContentKeyResponse:[PAL::getAVContentKeyResponseClass() contentKeyResponseWithFairPlayStreamingKeyResponseData:payloadData->makeContiguous()->createNSData().get()]];
            }
            return true;
        };
        for (auto value : *array) {
            if (!parseResponse(WTFMove(value))) {
                DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "'cenc' initData, Failed, could not parse response");
                callback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
                return;
            }
        }
    } else {
        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, "'sinf' initData, processing response");
        [m_currentRequest.value().requests.first() processContentKeyResponse:[PAL::getAVContentKeyResponseClass() contentKeyResponseWithFairPlayStreamingKeyResponseData:responseData->makeContiguous()->createNSData().get()]];
    }

    // FIXME(rdar://problem/35592277): stash the callback and call it once AVContentKeyResponse supports a success callback.
    struct objc_method_description method = protocol_getMethodDescription(@protocol(AVContentKeySessionDelegate), @selector(contentKeySession:contentKeyRequestDidSucceed:), NO, YES);
    if (!method.name) {
        KeyStatusVector keyStatuses;
        keyStatuses.reserveInitialCapacity(1);
        keyStatuses.uncheckedAppend(std::make_pair(WTFMove(keyIDs.first()), KeyStatus::Usable));
        callback(false, std::make_optional(WTFMove(keyStatuses)), std::nullopt, std::nullopt, Succeeded);
        return;
    }

    m_updateLicenseCallback = WTFMove(callback);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::loadSession(LicenseType licenseType, const String& sessionId, const String&, LoadSessionCallback&& callback)
{
    if (licenseType == LicenseType::PersistentUsageRecord) {
        auto* storageURL = m_instance->storageURL();
        if (!m_instance->persistentStateAllowed() || !storageURL) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, mismatched session type");
            callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::MismatchedSessionType);
            return;
        }
        auto* certificate = m_instance->serverCertificate();
        if (!certificate) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, no sessionCertificate");
            callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::NoSessionData);
            return;
        }

        RetainPtr<NSData> appIdentifier = certificate->makeContiguous()->createNSData();
        KeyStatusVector changedKeys;
        for (NSData* expiredSessionData in [PAL::getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:appIdentifier.get() storageDirectoryAtURL:storageURL]) {
            static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (sessionId == String(playbackSessionIdValue)) {
                // FIXME(rdar://problem/35934922): use key values stored in expired session report once available
                changedKeys.append((KeyStatusVector::ValueType){ SharedBuffer::create(), KeyStatus::Released });
                m_expiredSessions.append(expiredSessionData);
            }
        }

        if (changedKeys.isEmpty()) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, no session data found");
            callback(std::nullopt, std::nullopt, std::nullopt, Failed, SessionLoadFailure::NoSessionData);
            return;
        }

        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Succeeded, mismatched session type");
        callback(WTFMove(changedKeys), std::nullopt, std::nullopt, Succeeded, SessionLoadFailure::None);
    }
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::closeSession(const String&, CloseSessionCallback&& callback)
{
    DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (m_requestLicenseCallback) {
        m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
        ASSERT(!m_requestLicenseCallback);
    }
    if (m_updateLicenseCallback) {
        m_updateLicenseCallback(true, std::nullopt, std::nullopt, std::nullopt, Failed);
        ASSERT(!m_updateLicenseCallback);
    }
    if (m_removeSessionDataCallback) {
        m_removeSessionDataCallback({ }, nullptr, Failed);
        ASSERT(!m_removeSessionDataCallback);
    }
    m_currentRequest = std::nullopt;
    m_pendingRequests.clear();
    m_requests.clear();
    callback();
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::removeSessionData(const String& sessionId, LicenseType licenseType, RemoveSessionDataCallback&& callback)
{
    if (m_group)
        [m_group expire];
    else
        [m_session expire];

    if (licenseType == LicenseType::PersistentUsageRecord) {
        auto* storageURL = m_instance->storageURL();
        auto* certificate = m_instance->serverCertificate();

        if (!m_instance->persistentStateAllowed() || !storageURL || !certificate) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, persistentState not allowed or no storageURL or no certificate");
            callback({ }, std::nullopt, Failed);
            return;
        }

        RetainPtr<NSData> appIdentifier = certificate->makeContiguous()->createNSData();
        RetainPtr<NSMutableArray> expiredSessionsArray = adoptNS([[NSMutableArray alloc] init]);
        KeyStatusVector changedKeys;
        for (NSData* expiredSessionData in [PAL::getAVContentKeySessionClass() pendingExpiredSessionReportsWithAppIdentifier:appIdentifier.get() storageDirectoryAtURL:storageURL]) {
            NSDictionary *expiredSession = [NSPropertyListSerialization propertyListWithData:expiredSessionData options:kCFPropertyListImmutable format:nullptr error:nullptr];
            static const NSString *PlaybackSessionIdKey = @"PlaybackSessionID";
            NSString *playbackSessionIdValue = (NSString *)[expiredSession objectForKey:PlaybackSessionIdKey];
            if (![playbackSessionIdValue isKindOfClass:[NSString class]])
                continue;

            if (sessionId == String(playbackSessionIdValue)) {
                // FIXME(rdar://problem/35934922): use key values stored in expired session report once available
                changedKeys.append((KeyStatusVector::ValueType){ SharedBuffer::create(), KeyStatus::Released });
                m_expiredSessions.append(expiredSessionData);
                [expiredSessionsArray addObject:expiredSession];
            }
        }

        if (!expiredSessionsArray.get().count) {
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Succeeded, no expired sessions");
            callback(WTFMove(changedKeys), std::nullopt, Succeeded);
            return;
        }

        auto expiredSessionsCount = expiredSessionsArray.get().count;
        if (!expiredSessionsCount) {
            // It should not be possible to have a persistent-usage-record session that does not generate
            // a persistent-usage-record message on close. Signal this by failing and assert.
            ASSERT_NOT_REACHED();
            DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Failed, no expired session data");
            callback(WTFMove(changedKeys), std::nullopt, Failed);
            return;
        }

        id propertyList = expiredSessionsCount == 1 ? [expiredSessionsArray firstObject] : expiredSessionsArray.get();

        RetainPtr<NSData> expiredSessionsData = [NSPropertyListSerialization dataWithPropertyList:propertyList format:NSPropertyListBinaryFormat_v1_0 options:kCFPropertyListImmutable error:nullptr];

        if (expiredSessionsCount > 1)
            ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "Multiple(", expiredSessionsCount, ") expired session reports found with same sessionID(", sessionId, ")!");

        DEBUG_LOG_IF_POSSIBLE(LOGIDENTIFIER, " Succeeded");
        callback(WTFMove(changedKeys), SharedBuffer::create(expiredSessionsData.get()), Succeeded);
        return;
    }

    callback({ }, nullptr, Failed);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::storeRecordOfKeyUsage(const String&)
{
    // no-op; key usage data is stored automatically.
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::displayChanged(PlatformDisplayID displayID)
{
    updateProtectionStatusForDisplayID(displayID);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::setClient(WeakPtr<CDMInstanceSessionClient>&& client)
{
    m_client = WTFMove(client);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::clearClient()
{
    m_client = nullptr;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvideRequest(AVContentKeyRequest *request)
{
    auto initDataType = initTypeForRequest(request);
    if (initDataType == emptyAtom())
        return;

    Request currentRequest = { initDataType, { request } };

    if (m_currentRequest) {
        m_pendingRequests.append(WTFMove(currentRequest));
        return;
    }

    m_currentRequest = currentRequest;
    m_requests.append(WTFMove(currentRequest));

    RetainPtr<NSData> appIdentifier;
    if (auto* certificate = m_instance->serverCertificate())
        appIdentifier = certificate->makeContiguous()->createNSData();

    auto keyIDs = keyIDsForRequest(request);
    if (keyIDs.isEmpty()) {
        if (m_requestLicenseCallback) {
            m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
            ASSERT(!m_requestLicenseCallback);
        }
        return;
    }

    RetainPtr<NSData> contentIdentifier = keyIDs.first()->makeContiguous()->createNSData();
    @try {
        [request makeStreamingContentKeyRequestDataForApp:appIdentifier.get() contentIdentifier:contentIdentifier.get() options:nil completionHandler:[this, weakThis = WeakPtr { *this }] (NSData *contentKeyRequestData, NSError *error) mutable {
            callOnMainThread([this, weakThis = WTFMove(weakThis), error = retainPtr(error), contentKeyRequestData = retainPtr(contentKeyRequestData)] {
                if (!weakThis)
                    return;

                if (m_sessionId.isEmpty()) {
                    auto sessionID = m_group ? m_group.get().contentProtectionSessionIdentifier : m_session.get().contentProtectionSessionIdentifier;
                    sessionIdentifierChanged(sessionID);
                }

                if (error && m_requestLicenseCallback)
                    m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
                else if (m_requestLicenseCallback)
                    m_requestLicenseCallback(SharedBuffer::create(contentKeyRequestData.get()), m_sessionId, false, Succeeded);
                else if (m_client)
                    m_client->sendMessage(CDMMessageType::LicenseRequest, SharedBuffer::create(contentKeyRequestData.get()));
                ASSERT(!m_requestLicenseCallback);
            });
        }];
    } @catch(NSException *exception) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "exception thrown from -makeStreamingContentKeyRequestDataForApp:contentIdentifier:options:completionHandler: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        if (m_updateLicenseCallback)
            m_updateLicenseCallback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        ASSERT(!m_updateLicenseCallback);
    }
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvideRequests(Vector<RetainPtr<AVContentKeyRequest>>&& requests)
{
    if (requests.isEmpty())
        return;

    auto initDataType = initTypeForRequest(requests.first().get());
    if (initDataType != InitDataRegistry::cencName()) {
        didProvideRequest(requests.first().get());
        requests.remove(0);

        for (auto& request : requests)
            m_pendingRequests.append({ initDataType, { WTFMove(request) } });

        return;
    }

    Request currentRequest = { initDataType, WTFMove(requests) };
    if (m_currentRequest) {
        m_pendingRequests.append(WTFMove(currentRequest));
        return;
    }

    m_currentRequest = currentRequest;
    m_requests.append(WTFMove(currentRequest));

    RetainPtr<NSData> appIdentifier;
    if (auto* certificate = m_instance->serverCertificate())
        appIdentifier = certificate->makeContiguous()->createNSData();

    using RequestsData = Vector<std::pair<RefPtr<SharedBuffer>, RetainPtr<NSData>>>;
    struct CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator> {
        using CallbackFunction = Function<void(RequestsData&&)>;
        static RefPtr<CallbackAggregator> create(CallbackFunction&& completionHandler)
        {
            return adoptRef(new CallbackAggregator(WTFMove(completionHandler)));
        }

        explicit CallbackAggregator(Function<void(RequestsData&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            callOnMainThread([completionHandler = WTFMove(m_completionHandler), requestsData = WTFMove(requestsData)] () mutable {
                completionHandler(WTFMove(requestsData));
            });
        }

        CompletionHandler<void(RequestsData&&)> m_completionHandler;
        RequestsData requestsData;
    };

    auto aggregator = CallbackAggregator::create([this, weakThis = WeakPtr { *this }] (RequestsData&& requestsData) {
        if (!weakThis)
            return;

        if (!m_requestLicenseCallback)
            return;

        if (requestsData.isEmpty()) {
            m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
            return;
        }

        auto requestJSON = JSON::Array::create();
        for (auto& requestData : requestsData) {
            auto entry = JSON::Object::create();
            auto& keyID = requestData.first;
            auto& payload = requestData.second;
            entry->setString("keyID", base64EncodeToString(keyID->makeContiguous()->data(), keyID->size()));
            entry->setString("payload", base64EncodeToString(payload.get().bytes, payload.get().length));
            requestJSON->pushObject(WTFMove(entry));
        }
        auto requestBuffer = utf8Buffer(requestJSON->toJSONString());
        if (!requestBuffer) {
            m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
            return;
        }
        m_requestLicenseCallback(requestBuffer.releaseNonNull(), m_sessionId, false, Succeeded);
    });

    @try {
        for (auto request : m_currentRequest.value().requests) {
            auto keyIDs = keyIDsForRequest(request.get());
            RefPtr<SharedBuffer> keyID = WTFMove(keyIDs.first());
            auto contentIdentifier = keyID->makeContiguous()->createNSData();
            [request makeStreamingContentKeyRequestDataForApp:appIdentifier.get() contentIdentifier:contentIdentifier.get() options:nil completionHandler:[keyID = WTFMove(keyID), aggregator] (NSData *contentKeyRequestData, NSError *) mutable {
                callOnMainThread([keyID = WTFMove(keyID), aggregator = WTFMove(aggregator), contentKeyRequestData = retainPtr(contentKeyRequestData)] () mutable {
                    aggregator->requestsData.append({ WTFMove(keyID), WTFMove(contentKeyRequestData) });
                });
            }];
        }
    } @catch(NSException *exception) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "exception thrown from -makeStreamingContentKeyRequestDataForApp:contentIdentifier:options:completionHandler: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        if (m_requestLicenseCallback)
            m_requestLicenseCallback(SharedBuffer::create(), m_sessionId, false, Failed);
        ASSERT(!m_requestLicenseCallback);
    }
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvideRenewingRequest(AVContentKeyRequest *request)
{
    ASSERT(!m_requestLicenseCallback);
    auto initDataType = initTypeForRequest(request);
    if (initDataType == emptyAtom())
        return;

    Request currentRequest = { initDataType, { request } };
    if (m_currentRequest) {
        m_pendingRequests.append(WTFMove(currentRequest));
        return;
    }

    m_currentRequest = currentRequest;

    // The assumption here is that AVContentKeyRequest will only ever notify us of a renewing request as a result of calling
    // -renewExpiringResponseDataForContentKeyRequest: with an existing request.
    ASSERT(m_requests.contains(m_currentRequest));

    RetainPtr<NSData> appIdentifier;
    if (auto* certificate = m_instance->serverCertificate())
        appIdentifier = certificate->makeContiguous()->createNSData();
    auto keyIDs = keyIDsForRequest(m_currentRequest.value());

    RetainPtr<NSData> contentIdentifier = keyIDs.first()->makeContiguous()->createNSData();
    @try {
        [request makeStreamingContentKeyRequestDataForApp:appIdentifier.get() contentIdentifier:contentIdentifier.get() options:nil completionHandler:[this, weakThis = WeakPtr { *this }] (NSData *contentKeyRequestData, NSError *error) mutable {
            callOnMainThread([this, weakThis = WTFMove(weakThis), error = retainPtr(error), contentKeyRequestData = retainPtr(contentKeyRequestData)] {
                if (!weakThis || !m_client || error)
                    return;

                if (error && m_updateLicenseCallback)
                    m_updateLicenseCallback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
                else if (m_updateLicenseCallback)
                    m_updateLicenseCallback(false, std::nullopt, std::nullopt, Message(MessageType::LicenseRenewal, SharedBuffer::create(contentKeyRequestData.get())), Succeeded);
                else if (m_client)
                    m_client->sendMessage(CDMMessageType::LicenseRenewal, SharedBuffer::create(contentKeyRequestData.get()));
                ASSERT(!m_updateLicenseCallback);
            });
        }];
    } @catch(NSException *exception) {
        ERROR_LOG_IF_POSSIBLE(LOGIDENTIFIER, "exception thrown from -makeStreamingContentKeyRequestDataForApp:contentIdentifier:options:completionHandler: ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        if (m_updateLicenseCallback)
            m_updateLicenseCallback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        ASSERT(!m_updateLicenseCallback);
    }
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didProvidePersistableRequest(AVContentKeyRequest *)
{
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::didFailToProvideRequest(AVContentKeyRequest *request, NSError *error)
{
    // Rather than reject the update() promise when the CDM indicates that
    // the key requires a higher level of security than it is currently able
    // to provide, signal this state by "succeeding", but set the key status
    // to "output-restricted".

    if (error.code == SecurityLevelError) {
        requestDidSucceed(request);
        return;
    }

    if (m_updateResponseCollector) {
        m_updateResponseCollector->addErrorResponse(request, error);
        return;
    }

    if (m_updateLicenseCallback) {
        m_updateLicenseCallback(false, std::nullopt, std::nullopt, std::nullopt, Failed);
        ASSERT(!m_updateLicenseCallback);
    }

    m_currentRequest = std::nullopt;

    nextRequest();
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::requestDidSucceed(AVContentKeyRequest *request)
{
    if (m_updateResponseCollector) {
        m_updateResponseCollector->addSuccessfulResponse(request, nullptr);
        return;
    }

    if (m_updateLicenseCallback) {
        m_updateLicenseCallback(false, std::make_optional(keyStatuses()), std::nullopt, std::nullopt, Succeeded);
        ASSERT(!m_updateLicenseCallback);
    }

    m_currentRequest = std::nullopt;

    nextRequest();
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::nextRequest()
{
    if (m_pendingRequests.isEmpty())
        return;

    Request nextRequest = WTFMove(m_pendingRequests.first());
    m_pendingRequests.remove(0);

    if (nextRequest.requests.isEmpty())
        return;

    if (nextRequest.initType == InitDataRegistry::cencName()) {
        didProvideRequests(WTFMove(nextRequest.requests));
        return;
    }

    ASSERT(nextRequest.requests.size() == 1);
    auto* oneRequest = nextRequest.requests.first().get();
    if (oneRequest.renewsExpiringResponseData)
        didProvideRenewingRequest(oneRequest);
    else
        didProvideRequest(oneRequest);
}

AVContentKeyRequest* CDMInstanceSessionFairPlayStreamingAVFObjC::lastKeyRequest() const
{
    if (m_requests.isEmpty())
        return nil;
    auto& lastRequest = m_requests.last();
    if (lastRequest.requests.isEmpty())
        return nil;
    return lastRequest.requests.last().get();
}

bool CDMInstanceSessionFairPlayStreamingAVFObjC::hasRequest(AVContentKeyRequest* keyRequest) const
{
    for (auto& request : m_requests) {
        if (request.requests.contains(keyRequest))
            return true;
    }
    return false;
}

bool CDMInstanceSessionFairPlayStreamingAVFObjC::shouldRetryRequestForReason(AVContentKeyRequest *, NSString *)
{
    notImplemented();
    return false;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::sessionIdentifierChanged(NSData *sessionIdentifier)
{
    String sessionId = emptyString();
    if (sessionIdentifier)
        sessionId = adoptNS([[NSString alloc] initWithData:sessionIdentifier encoding:NSUTF8StringEncoding]).get();

    if (m_sessionId == sessionId)
        return;

    m_sessionId = sessionId;
    if (m_client)
        m_client->sessionIdChanged(m_sessionId);
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::groupSessionIdentifierChanged(AVContentKeyReportGroup*, NSData *sessionIdentifier)
{
    sessionIdentifierChanged(sessionIdentifier);
}

static auto requestStatusToCDMStatus(AVContentKeyRequestStatus status)
{
    switch (status) {
        case AVContentKeyRequestStatusRequestingResponse:
        case AVContentKeyRequestStatusRetried:
            return CDMKeyStatus::StatusPending;
        case AVContentKeyRequestStatusReceivedResponse:
        case AVContentKeyRequestStatusRenewed:
            return CDMKeyStatus::Usable;
        case AVContentKeyRequestStatusCancelled:
            return CDMKeyStatus::Released;
        case AVContentKeyRequestStatusFailed:
            return CDMKeyStatus::InternalError;
    }
}

CDMInstanceSession::KeyStatusVector CDMInstanceSessionFairPlayStreamingAVFObjC::keyStatuses(std::optional<PlatformDisplayID> displayID) const
{
    KeyStatusVector keyStatuses;

    if (!displayID && m_client)
        displayID = m_client->displayID();

    for (auto& request : m_requests) {
        for (auto& oneRequest : request.requests) {
            auto keyIDs = keyIDsForRequest(oneRequest.get());
            auto status = requestStatusToCDMStatus(oneRequest.get().status);
            if (oneRequest.get().error.code == SecurityLevelError)
                status = CDMKeyStatus::OutputRestricted;

            if (auto protectionStatus = protectionStatusForDisplayID(oneRequest.get(), *displayID))
                status = *protectionStatus;

            for (auto& keyID : keyIDs)
                keyStatuses.append({ WTFMove(keyID), status });
        }
    }

    return keyStatuses;
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::outputObscuredDueToInsufficientExternalProtectionChanged(bool obscured)
{
    if (obscured == m_outputObscured)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, obscured);
    m_outputObscured = obscured;

    if (m_client)
        m_client->updateKeyStatuses(keyStatuses());
}

void CDMInstanceSessionFairPlayStreamingAVFObjC::externalProtectionStatusDidChangeForContentKeyRequest(AVContentKeyRequest*)
{
    if (!m_client)
        return;

    updateProtectionStatusForDisplayID(m_client->displayID());
}

std::optional<CDMKeyStatus> CDMInstanceSessionFairPlayStreamingAVFObjC::protectionStatusForDisplayID(AVContentKeyRequest *request, std::optional<PlatformDisplayID>) const
{
#if HAVE(AVCONTENTKEYREQUEST_PENDING_PROTECTION_STATUS)
    if ([request respondsToSelector:@selector(externalContentProtectionStatus)]) {
        switch ([request externalContentProtectionStatus]) {
        case AVExternalContentProtectionStatusPending: return CDMKeyStatus::StatusPending;
        case AVExternalContentProtectionStatusSufficient: return CDMKeyStatus::Usable;
        case AVExternalContentProtectionStatusInsufficient: return CDMKeyStatus::OutputRestricted;
        }
    }
#endif

    // FIXME: AVFoundation requires a connection to the WindowServer in order to query the HDCP status of individual
    // displays. Passing in an empty NSArray will cause AVFoundation to fall back to a "minimum supported HDCP level"
    // across all displays. Replace the below with explicit APIs to query the per-display HDCP status in the UIProcess
    // and to query the HDCP level required by each AVContentKeyRequest, and do the comparison between the two in the
    // WebProcess.
    if ([request respondsToSelector:@selector(willOutputBeObscuredDueToInsufficientExternalProtectionForDisplays:)]) {

        // willOutputBeObscuredDueToInsufficientExternalProtectionForDisplays will always return "YES" prior to
        // receiving a response.
        if (request.status != AVContentKeyRequestStatusReceivedResponse && request.status != AVContentKeyRequestStatusRenewed) {
            ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "request has insufficient status ", (int)request.status);
            return std::nullopt;
        }

        auto obscured = [request willOutputBeObscuredDueToInsufficientExternalProtectionForDisplays:@[ ]];
        ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "request { ", keyIDsForRequest(request), " } willOutputBeObscured...forDisplays:[ nil ] = ", obscured ? "true" : "false");
        return obscured ? CDMKeyStatus::OutputRestricted : CDMKeyStatus::Usable;
    }

    // Only use the non-request-specific "outputObscuredDueToInsufficientExternalProtection" status if
    // AVContentKeyRequests do not support the finer grained "-willOutputBeObscured..." API.
    if (m_outputObscured)
        return CDMKeyStatus::OutputRestricted;

    return std::nullopt;
};


void CDMInstanceSessionFairPlayStreamingAVFObjC::updateProtectionStatusForDisplayID(PlatformDisplayID displayID)
{
    if (m_requests.isEmpty() || !m_client)
        return;
    m_client->updateKeyStatuses(keyStatuses(displayID));
}

bool CDMInstanceSessionFairPlayStreamingAVFObjC::ensureSessionOrGroup()
{
    if (m_session || m_group)
        return true;

    if (auto* session = m_instance->contentKeySession()) {
        m_group = [session makeContentKeyGroup];
        return true;
    }

    auto storageURL = m_instance->storageURL();
    if (!m_instance->persistentStateAllowed() || !storageURL)
        m_session = [PAL::getAVContentKeySessionClass() contentKeySessionWithKeySystem:AVContentKeySystemFairPlayStreaming];
    else
        m_session = [PAL::getAVContentKeySessionClass() contentKeySessionWithKeySystem:AVContentKeySystemFairPlayStreaming storageDirectoryAtURL:storageURL];

    if (!m_session)
        return false;

    [m_session setDelegate:m_delegate.get() queue:dispatch_get_main_queue()];
    return true;
}

bool CDMInstanceSessionFairPlayStreamingAVFObjC::isLicenseTypeSupported(LicenseType licenseType) const
{
    switch (licenseType) {
    case CDMSessionType::PersistentLicense:
        return m_instance->persistentStateAllowed() && m_instance->supportsPersistentKeys();
    case CDMSessionType::PersistentUsageRecord:
        return m_instance->persistentStateAllowed() && m_instance->supportsPersistableState();
    case CDMSessionType::Temporary:
        return true;
    }
}

}

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
