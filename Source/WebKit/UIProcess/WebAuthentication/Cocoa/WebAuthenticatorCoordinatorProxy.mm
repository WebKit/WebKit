/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if HAVE(UNIFIED_ASC_AUTH_UI)

#import "config.h"
#import "WebAuthenticatorCoordinatorProxy.h"

#import "LocalService.h"
#import "Logging.h"
#import "PageClient.h"
#import "WKError.h"
#import "WKWebView.h"
#import "WebAuthenticationRequestData.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <WebCore/AuthenticatorAttachment.h>
#import <WebCore/AuthenticatorResponseData.h>
#import <WebCore/AuthenticatorTransport.h>
#import <WebCore/BufferSource.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/EnumTraits.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

#import "AuthenticationServicesCoreSoftLink.h"
#if HAVE(WEB_AUTHN_AS_MODERN)
#import "AuthenticationServicesSoftLink.h"

@interface _WKASDelegate : NSObject {
    WeakPtr<WebKit::WebPageProxy> m_page;
    BlockPtr<void(ASAuthorization *, NSError *)> m_completionHandler;
}
- (instancetype)initWithPage:(WeakPtr<WebKit::WebPageProxy> &&)page completionHandler:(BlockPtr<void(ASAuthorization *, NSError *)> &&)completionHandler;
@end

@implementation _WKASDelegate
- (instancetype)initWithPage:(WeakPtr<WebKit::WebPageProxy> &&)page completionHandler:(BlockPtr<void(ASAuthorization *, NSError *)> &&)completionHandler
{
    if (!(self = [super init]))
        return nil;

    m_page = WTFMove(page);
    m_completionHandler = WTFMove(completionHandler);

    return self;
}

- (ASPresentationAnchor)presentationAnchorForAuthorizationController:(ASAuthorizationController *)controller
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return [m_page->cocoaView() window];
#pragma clang diagnostic pop
}

- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithAuthorization:(ASAuthorization *)authorization
{
    m_completionHandler(authorization, nil);
}
- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithError:(NSError *)error
{
    m_completionHandler(nil, error);
}
@end

#endif // HAVE(WEB_AUTHN_AS_MODERN)

namespace WebKit {
using namespace WebCore;

static inline Ref<ArrayBuffer> toArrayBuffer(NSData *data)
{
    return ArrayBuffer::create(reinterpret_cast<const uint8_t *>(data.bytes), data.length);
}

#if HAVE(WEB_AUTHN_AS_MODERN)

static inline RetainPtr<NSString> toASUserVerificationPreference(WebCore::UserVerificationRequirement requirement)
{
    switch (requirement) {
    case WebCore::UserVerificationRequirement::Preferred:
        return @"preferred";
    case WebCore::UserVerificationRequirement::Discouraged:
        return @"discouraged";
    case WebCore::UserVerificationRequirement::Required:
        return @"required";
    default:
        return @"required";
    }
}

static inline RetainPtr<NSString> toAttestationConveyancePreference(AttestationConveyancePreference attestationConveyancePreference)
{
    switch (attestationConveyancePreference) {
    case AttestationConveyancePreference::Direct:
        return @"direct";
    case AttestationConveyancePreference::Indirect:
        return @"indirect";
    case AttestationConveyancePreference::None:
        return @"none";
    case AttestationConveyancePreference::Enterprise:
        return @"enterprise";
    }

    return @"none";
}

static inline RetainPtr<NSString> toASResidentKeyPreference(std::optional<ResidentKeyRequirement> requirement, bool requireResidentKey)
{
    if (!requirement)
        return requireResidentKey ? @"required" : @"discouraged";
    switch (*requirement) {
    case ResidentKeyRequirement::Discouraged:
        return @"discouraged";
    case ResidentKeyRequirement::Preferred:
        return @"preferred";
    case ResidentKeyRequirement::Required:
        return @"required";
    }
    ASSERT_NOT_REACHED();
    return @"preferred";
}

static inline RetainPtr<NSString> toASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport(WebCore::AuthenticatorTransport transport)
{
    switch (transport) {
    case WebCore::AuthenticatorTransport::Ble:
        return @"ble";
    case WebCore::AuthenticatorTransport::Nfc:
        return @"nfc";
    case WebCore::AuthenticatorTransport::Usb:
        return @"usb";
    default:
        return nil;
    }
}

RetainPtr<ASAuthorizationController> WebAuthenticatorCoordinatorProxy::constructASController(WebAuthenticationRequestData &&requestData)
{
    RetainPtr<NSArray> requests;
    WTF::switchOn(requestData.options, [&](const PublicKeyCredentialCreationOptions &options) {
        requests = requestsForRegisteration(options, requestData.hash);
    }, [&](const PublicKeyCredentialRequestOptions &options) {
        requests = requestsForAssertion(options, requestData.hash, requestData.parentOrigin);
    });
    if (!requests || ![requests count])
        return nullptr;
    return adoptNS([allocASAuthorizationControllerInstance() initWithAuthorizationRequests:requests.get()]);
}

RetainPtr<NSArray> WebAuthenticatorCoordinatorProxy::requestsForRegisteration(const PublicKeyCredentialCreationOptions &options, const Vector<uint8_t> &hash)
{
    RetainPtr<NSMutableArray<ASAuthorizationRequest *>> requests = adoptNS([[NSMutableArray alloc] init]);
    bool includeSecurityKeyRequest = true;
    bool includePlatformRequest = true;
    if (options.authenticatorSelection) {
        if (auto attachment = options.authenticatorSelection->authenticatorAttachment) {
            switch (*attachment) {
            case WebCore::AuthenticatorAttachment::Platform:
                includeSecurityKeyRequest = false;
                break;
            case WebCore::AuthenticatorAttachment::CrossPlatform:
                includePlatformRequest = false;
                break;
            default:
                break;
            }
        }
    }
    if (includePlatformRequest) {
        auto request = adoptNS([[allocASAuthorizationPlatformPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:*options.rp.id] createCredentialRegistrationRequestWithChallenge:toNSData(options.challenge).get() name:options.user.name userID:toNSData(options.user.id).get()]);
        request.get().attestationPreference = toAttestationConveyancePreference(options.attestation).get();
        if (options.authenticatorSelection)
            request.get().userVerificationPreference = toASUserVerificationPreference(options.authenticatorSelection->userVerification).get();
        [requests addObject:request.leakRef()];
    }
    if (includeSecurityKeyRequest) {
        auto request = adoptNS([[allocASAuthorizationSecurityKeyPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:*options.rp.id] createCredentialRegistrationRequestWithChallenge:toNSData(options.challenge).get() displayName:options.user.displayName name:options.user.name userID:toNSData(options.user.id).get()]);
        request.get().attestationPreference = toAttestationConveyancePreference(options.attestation).get();
        RetainPtr<NSMutableArray<ASAuthorizationPublicKeyCredentialParameters *>> parameters = adoptNS([[NSMutableArray alloc] init]);
        for (auto alg : options.pubKeyCredParams)
            [parameters addObject:adoptNS([allocASAuthorizationPublicKeyCredentialParametersInstance() initWithAlgorithm:alg.alg]).get()];
        request.get().credentialParameters = parameters.get();
        if (options.authenticatorSelection) {
            request.get().userVerificationPreference = toASUserVerificationPreference(options.authenticatorSelection->userVerification).get();
            request.get().residentKeyPreference = toASResidentKeyPreference(options.authenticatorSelection->residentKey, options.authenticatorSelection->requireResidentKey).get();
        }
        [requests addObject:request.leakRef()];
    }

    return requests;
}

RetainPtr<NSArray> WebAuthenticatorCoordinatorProxy::requestsForAssertion(const PublicKeyCredentialRequestOptions &options, const Vector<uint8_t> &hash, std::optional<WebCore::SecurityOriginData> &parentOrigin)
{
    RetainPtr<NSMutableArray<ASAuthorizationRequest *>> requests = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<ASAuthorizationPlatformPublicKeyCredentialDescriptor *>> platformAllowedCredentials = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *>> crossPlatformAllowedCredentials = adoptNS([[NSMutableArray alloc] init]);
    for (auto credential : options.allowCredentials) {
        if (credential.transports.contains(AuthenticatorTransport::Internal) || credential.transports.isEmpty())
            [platformAllowedCredentials addObject:adoptNS([allocASAuthorizationPlatformPublicKeyCredentialDescriptorInstance() initWithCredentialID:toNSData(credential.id).get()]).get()];
        if (credential.transports.isEmpty() || !credential.transports.contains(AuthenticatorTransport::Internal)) {
            RetainPtr<NSMutableArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport>> transports = adoptNS([[NSMutableArray alloc] init]);
            for (auto transport : credential.transports) {
                if (auto asTransport = toASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport(transport))
                    [transports addObject:asTransport.get()];
            }
            [crossPlatformAllowedCredentials addObject:adoptNS([allocASAuthorizationSecurityKeyPublicKeyCredentialDescriptorInstance() initWithCredentialID:toNSData(credential.id).get() transports:transports.get()]).get()];
        }
    }
    if ([platformAllowedCredentials count] || ![crossPlatformAllowedCredentials count]) {
        auto request = adoptNS([[allocASAuthorizationPlatformPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:options.rpId] createCredentialAssertionRequestWithChallenge:toNSData(options.challenge).get()]);
        if (platformAllowedCredentials)
            request.get().allowedCredentials = platformAllowedCredentials.get();
        [requests addObject:request.leakRef()];
    }

    if ([crossPlatformAllowedCredentials count] || ![platformAllowedCredentials count]) {
        auto request = adoptNS([[allocASAuthorizationSecurityKeyPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:options.rpId] createCredentialAssertionRequestWithChallenge:toNSData(options.challenge).get()]);

        if (crossPlatformAllowedCredentials)
            request.get().allowedCredentials = crossPlatformAllowedCredentials.get();
        [requests addObject:request.leakRef()];
    }

    return requests;
}

void WebAuthenticatorCoordinatorProxy::pauseConditionalAssertion()
{
    if (m_paused)
        return;
    m_paused = true;
    if (m_isConditionalAssertion)
        [m_controller cancel];
}

void WebAuthenticatorCoordinatorProxy::unpauseConditionalAssertion()
{
    if (!m_paused)
        return;
    if (m_controller && m_isConditionalAssertion)
        [m_controller performAutoFillAssistedRequests];

    m_paused = false;
}

#endif // HAVE(WEB_AUTHN_AS_MODERN)

void WebAuthenticatorCoordinatorProxy::performRequest(WebAuthenticationRequestData &&requestData, RequestCompletionHandler &&handler)
{
    if (!m_webPageProxy.preferences().webAuthenticationASEnabled()) {
        auto context = contextForRequest(WTFMove(requestData));
        if (context.get() == nullptr) {
            handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
            RELEASE_LOG_ERROR(WebAuthn, "The origin of the document is not the same as its ancestors.");
            return;
        }
        performRequestLegacy(context, WTFMove(handler));
        return;
    }
#if HAVE(WEB_AUTHN_AS_MODERN)
    m_isConditionalAssertion = *requestData.mediation == MediationRequirement::Conditional;
    auto controller = constructASController(WTFMove(requestData));
    if (!controller) {
        handler(WebCore::AuthenticatorResponseData { }, AuthenticatorAttachment::Platform, { ExceptionCode::NotAllowedError, @"" });
        return;
    }
    m_controller = WTFMove(controller);
    m_completionHandler = WTFMove(handler);
    m_delegate = adoptNS([[_WKASDelegate alloc] initWithPage:WTFMove(requestData.page) completionHandler:makeBlockPtr([this](ASAuthorization *auth, NSError *error) mutable {
        ensureOnMainRunLoop([this, auth = retainPtr(auth)]() {
            WebCore::AuthenticatorResponseData response = { };
            WebCore::ExceptionData exceptionData = { ExceptionCode::NotAllowedError, @"" };
            WebCore::AuthenticatorAttachment attachment = AuthenticatorAttachment::Platform;
            if ([auth.get().credential isKindOfClass:getASAuthorizationPlatformPublicKeyCredentialRegistrationClass()]) {
                response.isAuthenticatorAttestationResponse = true;
                auto credential = retainPtr((ASAuthorizationPlatformPublicKeyCredentialRegistration *)auth.get().credential);
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.attestationObject = toArrayBuffer(credential.get().rawAttestationObject);
                response.transports = { };
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationPlatformPublicKeyCredentialAssertionClass()]) {
                auto credential = retainPtr((ASAuthorizationPlatformPublicKeyCredentialAssertion *)auth.get().credential);
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.authenticatorData = toArrayBuffer(credential.get().rawAuthenticatorData);
                response.signature = toArrayBuffer(credential.get().signature);
                response.userHandle = toArrayBuffer(credential.get().userID);
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationSecurityKeyPublicKeyCredentialRegistrationClass()]) {
                auto credential = retainPtr((ASAuthorizationSecurityKeyPublicKeyCredentialRegistration *)auth.get().credential);
                response.isAuthenticatorAttestationResponse = true;
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.attestationObject = toArrayBuffer(credential.get().rawAttestationObject);
                response.transports = { };
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationSecurityKeyPublicKeyCredentialAssertionClass()]) {
                auto credential = retainPtr((ASAuthorizationSecurityKeyPublicKeyCredentialAssertion *)auth.get().credential);
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.authenticatorData = toArrayBuffer(credential.get().rawAuthenticatorData);
                response.signature = toArrayBuffer(credential.get().signature);
                response.userHandle = toArrayBuffer(credential.get().userID);
            }
            if (!m_paused) {
                m_completionHandler(response, attachment, exceptionData);
                m_delegate.clear();
                m_controller.clear();
                m_isConditionalAssertion = false;
            }
        });
    }).get()]);

    m_controller.get().presentationContextProvider = (id<ASAuthorizationControllerPresentationContextProviding>)m_delegate.get();
    m_controller.get().delegate = (id<ASAuthorizationControllerDelegate>)m_delegate.get();
    if (requestData.mediation && *requestData.mediation == MediationRequirement::Conditional)
        [m_controller performAutoFillAssistedRequests];
    else
        [m_controller performRequests];
#endif
}

static inline RetainPtr<NSData> toNSData(const Vector<uint8_t>& data)
{
    return adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]);
}

static inline RetainPtr<NSString> toNSString(UserVerificationRequirement userVerificationRequirement)
{
    switch (userVerificationRequirement) {
    case UserVerificationRequirement::Required:
        return @"required";
    case UserVerificationRequirement::Discouraged:
        return @"discouraged";
    case UserVerificationRequirement::Preferred:
        return @"preferred";
    }

    return @"preferred";
}

static inline RetainPtr<NSString> toNSString(AttestationConveyancePreference attestationConveyancePreference)
{
    switch (attestationConveyancePreference) {
    case AttestationConveyancePreference::Direct:
        return @"direct";
    case AttestationConveyancePreference::Indirect:
        return @"indirect";
    case AttestationConveyancePreference::None:
        return @"none";
    case AttestationConveyancePreference::Enterprise:
        return @"enterprise";
    }

    return @"none";
}

static inline ExceptionCode toExceptionCode(NSInteger nsErrorCode)
{
    ExceptionCode exceptionCode = (ExceptionCode)nsErrorCode;

    switch (exceptionCode) {
    case ExceptionCode::IndexSizeError: FALLTHROUGH;
    case ExceptionCode::HierarchyRequestError: FALLTHROUGH;
    case ExceptionCode::WrongDocumentError: FALLTHROUGH;
    case ExceptionCode::InvalidCharacterError: FALLTHROUGH;
    case ExceptionCode::NoModificationAllowedError: FALLTHROUGH;
    case ExceptionCode::NotFoundError: FALLTHROUGH;
    case ExceptionCode::NotSupportedError: FALLTHROUGH;
    case ExceptionCode::InUseAttributeError: FALLTHROUGH;
    case ExceptionCode::InvalidStateError: FALLTHROUGH;
    case ExceptionCode::SyntaxError: FALLTHROUGH;
    case ExceptionCode::InvalidModificationError: FALLTHROUGH;
    case ExceptionCode::NamespaceError: FALLTHROUGH;
    case ExceptionCode::InvalidAccessError: FALLTHROUGH;
    case ExceptionCode::TypeMismatchError: FALLTHROUGH;
    case ExceptionCode::SecurityError: FALLTHROUGH;
    case ExceptionCode::NetworkError: FALLTHROUGH;
    case ExceptionCode::AbortError: FALLTHROUGH;
    case ExceptionCode::URLMismatchError: FALLTHROUGH;
    case ExceptionCode::QuotaExceededError: FALLTHROUGH;
    case ExceptionCode::TimeoutError: FALLTHROUGH;
    case ExceptionCode::InvalidNodeTypeError: FALLTHROUGH;
    case ExceptionCode::DataCloneError: FALLTHROUGH;
    case ExceptionCode::EncodingError: FALLTHROUGH;
    case ExceptionCode::NotReadableError: FALLTHROUGH;
    case ExceptionCode::UnknownError: FALLTHROUGH;
    case ExceptionCode::ConstraintError: FALLTHROUGH;
    case ExceptionCode::DataError: FALLTHROUGH;
    case ExceptionCode::TransactionInactiveError: FALLTHROUGH;
    case ExceptionCode::ReadonlyError: FALLTHROUGH;
    case ExceptionCode::VersionError: FALLTHROUGH;
    case ExceptionCode::OperationError: FALLTHROUGH;
    case ExceptionCode::NotAllowedError: FALLTHROUGH;
    case ExceptionCode::RangeError: FALLTHROUGH;
    case ExceptionCode::TypeError: FALLTHROUGH;
    case ExceptionCode::JSSyntaxError: FALLTHROUGH;
    case ExceptionCode::StackOverflowError: FALLTHROUGH;
    case ExceptionCode::OutOfMemoryError: FALLTHROUGH;
    case ExceptionCode::ExistingExceptionError:
        return exceptionCode;
    }

    return ExceptionCode::NotAllowedError;
}

static inline RetainPtr<ASCPublicKeyCredentialDescriptor> toASCDescriptor(PublicKeyCredentialDescriptor descriptor)
{
    RetainPtr<NSMutableArray<NSString *>> transports;
    size_t transportCount = descriptor.transports.size();
    if (transportCount) {
        transports = adoptNS([[NSMutableArray alloc] initWithCapacity:transportCount]);

        for (AuthenticatorTransport transport : descriptor.transports) {
            NSString *transportString = nil;
            switch (transport) {
            case AuthenticatorTransport::Usb:
                transportString = @"usb";
                break;
            case AuthenticatorTransport::Nfc:
                transportString = @"nfc";
                break;
            case AuthenticatorTransport::Ble:
                transportString = @"ble";
                break;
            case AuthenticatorTransport::Internal:
                transportString = @"internal";
                break;
            case AuthenticatorTransport::Cable:
                transportString = @"cable";
                break;
            case AuthenticatorTransport::Hybrid:
                transportString = @"hybrid";
                break;
            case AuthenticatorTransport::SmartCard:
                transportString = @"smart-card";
                break;
            }

            if (transportString)
                [transports addObject:transportString];
        }
    }

    return adoptNS([allocASCPublicKeyCredentialDescriptorInstance() initWithCredentialID:WebCore::toNSData(descriptor.id).get() transports:transports.get()]);
}

static inline RetainPtr<ASCWebAuthenticationExtensionsClientInputs> toASCExtensions(const AuthenticationExtensionsClientInputs& extensions)
{
    if ([allocASCWebAuthenticationExtensionsClientInputsInstance() respondsToSelector:@selector(initWithAppID:)])
        return adoptNS([allocASCWebAuthenticationExtensionsClientInputsInstance() initWithAppID:extensions.appid]);

    return nil;
}

static inline void setGlobalFrameIDForContext(RetainPtr<ASCCredentialRequestContext> requestContext, std::optional<WebCore::GlobalFrameIdentifier> globalFrameID)
{
    if (globalFrameID && [requestContext respondsToSelector:@selector(setGlobalFrameID:)]) {
        auto asGlobalFrameID = adoptNS([allocASGlobalFrameIdentifierInstance() initWithPageID:[NSNumber numberWithUnsignedLong:globalFrameID->pageID.toUInt64()] frameID:[NSNumber numberWithUnsignedLong:globalFrameID->frameID.object().toUInt64()]]);
        requestContext.get().globalFrameID = asGlobalFrameID.get();
    }
}

static inline ASPublicKeyCredentialResidentKeyPreference toASCResidentKeyPreference(std::optional<ResidentKeyRequirement> requirement, bool requireResidentKey)
{
    if (!requirement)
        return requireResidentKey ? ASPublicKeyCredentialResidentKeyPreferenceRequired : ASPublicKeyCredentialResidentKeyPreferenceNotPresent;
    switch (*requirement) {
    case ResidentKeyRequirement::Discouraged:
        return ASPublicKeyCredentialResidentKeyPreferenceDiscouraged;
    case ResidentKeyRequirement::Preferred:
        return ASPublicKeyCredentialResidentKeyPreferencePreferred;
    case ResidentKeyRequirement::Required:
        return ASPublicKeyCredentialResidentKeyPreferenceRequired;
    }
    ASSERT_NOT_REACHED();
    return ASPublicKeyCredentialResidentKeyPreferenceNotPresent;
}

static RetainPtr<ASCCredentialRequestContext> configureRegistrationRequestContext(const PublicKeyCredentialCreationOptions& options, const Vector<uint8_t>& hash, std::optional<WebCore::GlobalFrameIdentifier> globalFrameID, std::optional<WebCore::MediationRequirement> mediation)
{
    ASCCredentialRequestTypes requestTypes = ASCCredentialRequestTypePlatformPublicKeyRegistration | ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration;

    RetainPtr<NSString> userVerification;
    bool shouldRequireResidentKey = false;
    std::optional<ResidentKeyRequirement> residentKeyRequirement;
    std::optional<PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria> authenticatorSelection = options.authenticatorSelection;
    if (authenticatorSelection) {
        std::optional<AuthenticatorAttachment> attachment = authenticatorSelection->authenticatorAttachment;
        if (attachment == AuthenticatorAttachment::Platform)
            requestTypes = ASCCredentialRequestTypePlatformPublicKeyRegistration;
        else if (attachment == AuthenticatorAttachment::CrossPlatform)
            requestTypes = ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration;

        userVerification = toNSString(authenticatorSelection->userVerification);

        shouldRequireResidentKey = authenticatorSelection->requireResidentKey;
        residentKeyRequirement = authenticatorSelection->residentKey;
    }
    if (!LocalService::isAvailable())
        requestTypes &= ~ASCCredentialRequestTypePlatformPublicKeyRegistration;

    auto requestContext = adoptNS([allocASCCredentialRequestContextInstance() initWithRequestTypes:requestTypes]);
    ASSERT(options.rp.id);
    [requestContext setRelyingPartyIdentifier:*options.rp.id];
    setGlobalFrameIDForContext(requestContext, globalFrameID);

    auto credentialCreationOptions = adoptNS([allocASCPublicKeyCredentialCreationOptionsInstance() init]);

    if ([credentialCreationOptions respondsToSelector:@selector(setClientDataHash:)])
        [credentialCreationOptions setClientDataHash:toNSData(hash).get()];
    else
        [credentialCreationOptions setChallenge:WebCore::toNSData(options.challenge).get()];
    [credentialCreationOptions setRelyingPartyIdentifier:*options.rp.id];
    [credentialCreationOptions setUserName:options.user.name];
    [credentialCreationOptions setUserIdentifier:WebCore::toNSData(options.user.id).get()];
    [credentialCreationOptions setUserDisplayName:options.user.displayName];
    [credentialCreationOptions setUserVerificationPreference:userVerification.get()];
    if ([credentialCreationOptions respondsToSelector:@selector(setResidentKeyPreference:)])
        [credentialCreationOptions setResidentKeyPreference:toASCResidentKeyPreference(residentKeyRequirement, shouldRequireResidentKey)];
    else
        [credentialCreationOptions setShouldRequireResidentKey:shouldRequireResidentKey];
    [credentialCreationOptions setAttestationPreference:toNSString(options.attestation).get()];

    RetainPtr<NSMutableArray<NSNumber *>> supportedAlgorithmIdentifiers = adoptNS([[NSMutableArray alloc] initWithCapacity:options.pubKeyCredParams.size()]);
    for (PublicKeyCredentialCreationOptions::Parameters algorithmParameter : options.pubKeyCredParams)
        [supportedAlgorithmIdentifiers addObject:@(algorithmParameter.alg)];

    [credentialCreationOptions setSupportedAlgorithmIdentifiers:supportedAlgorithmIdentifiers.get()];

    size_t excludedCredentialCount = options.excludeCredentials.size();
    if (excludedCredentialCount) {
        RetainPtr<NSMutableArray<ASCPublicKeyCredentialDescriptor *>> excludedCredentials = adoptNS([[NSMutableArray alloc] initWithCapacity:excludedCredentialCount]);

        for (PublicKeyCredentialDescriptor descriptor : options.excludeCredentials)
            [excludedCredentials addObject:toASCDescriptor(descriptor).get()];

        [credentialCreationOptions setExcludedCredentials:excludedCredentials.get()];
    }
    
    if (options.extensions) {
        if ([credentialCreationOptions respondsToSelector:@selector(setExtensionsCBOR:)])
            [credentialCreationOptions setExtensionsCBOR:toNSData(options.extensions->toCBOR()).get()];
        else
            [credentialCreationOptions setExtensions:toASCExtensions(*options.extensions).get()];
    }

    if (options.timeout && [credentialCreationOptions respondsToSelector:@selector(setTimeout:)])
        credentialCreationOptions.get().timeout = [NSNumber numberWithUnsignedInt:*options.timeout];

    if (requestTypes & ASCCredentialRequestTypePlatformPublicKeyRegistration)
        [requestContext setPlatformKeyCredentialCreationOptions:credentialCreationOptions.get()];

    if (requestTypes & ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration)
        [requestContext setSecurityKeyCredentialCreationOptions:credentialCreationOptions.get()];

    if (mediation == MediationRequirement::Conditional)
        requestContext.get().requestStyle = ASCredentialRequestStyleSilent;

    return requestContext;
}

static inline RetainPtr<ASCPublicKeyCredentialAssertionOptions> configureAssertionOptions(const PublicKeyCredentialRequestOptions& options, const Vector<uint8_t>& hash, ASCPublicKeyCredentialKind kind, const std::optional<SecurityOriginData>& parentOrigin, RetainPtr<NSMutableArray<ASCPublicKeyCredentialDescriptor *>> allowedCredentials, RetainPtr<NSString> userVerification)
{
    auto assertionOptions = adoptNS(allocASCPublicKeyCredentialAssertionOptionsInstance());
    if ([assertionOptions respondsToSelector:@selector(initWithKind:relyingPartyIdentifier:clientDataHash:userVerificationPreference:allowedCredentials:)]) {
        auto nsHash = toNSData(hash);
        [assertionOptions initWithKind:kind relyingPartyIdentifier:options.rpId clientDataHash:nsHash.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get()];
    } else {
        auto challenge = WebCore::toNSData(options.challenge);
        [assertionOptions initWithKind:kind relyingPartyIdentifier:options.rpId challenge:challenge.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get()];
    }
    if (options.extensions) {
        if ([assertionOptions respondsToSelector:@selector(setExtensionsCBOR:)])
            [assertionOptions setExtensionsCBOR:toNSData(options.extensions->toCBOR()).get()];
        else
            [assertionOptions setExtensions:toASCExtensions(*options.extensions).get()];
    }
    if (parentOrigin && [assertionOptions respondsToSelector:@selector(setDestinationSiteForCrossSiteAssertion:)])
        assertionOptions.get().destinationSiteForCrossSiteAssertion = RegistrableDomain { *parentOrigin }.string();
    else if (parentOrigin && ![assertionOptions respondsToSelector:@selector(setDestinationSiteForCrossSiteAssertion:)])
        return nil;
    if (options.timeout && [assertionOptions respondsToSelector:@selector(setTimeout:)])
        assertionOptions.get().timeout = [NSNumber numberWithUnsignedInt:*options.timeout];
    return assertionOptions;
}

static RetainPtr<ASCCredentialRequestContext> configurationAssertionRequestContext(const PublicKeyCredentialRequestOptions& options, const Vector<uint8_t>& hash, std::optional<WebCore::MediationRequirement> mediation, std::optional<WebCore::GlobalFrameIdentifier> globalFrameID, std::optional<WebCore::SecurityOriginData>& parentOrigin)
{
    ASCCredentialRequestTypes requestTypes = ASCCredentialRequestTypePlatformPublicKeyAssertion | ASCCredentialRequestTypeSecurityKeyPublicKeyAssertion;

    RetainPtr<NSString> userVerification;
    std::optional<AuthenticatorAttachment> attachment = options.authenticatorAttachment;
    if (attachment == AuthenticatorAttachment::Platform)
        requestTypes = ASCCredentialRequestTypePlatformPublicKeyAssertion;
    else if (attachment == AuthenticatorAttachment::CrossPlatform)
        requestTypes = ASCCredentialRequestTypeSecurityKeyPublicKeyAssertion;

    userVerification = toNSString(options.userVerification);

    size_t allowedCredentialCount = options.allowCredentials.size();
    RetainPtr<NSMutableArray<ASCPublicKeyCredentialDescriptor *>> allowedCredentials;
    if (allowedCredentialCount) {
        allowedCredentials = adoptNS([[NSMutableArray alloc] initWithCapacity:allowedCredentialCount]);

        for (PublicKeyCredentialDescriptor descriptor : options.allowCredentials)
            [allowedCredentials addObject:toASCDescriptor(descriptor).get()];
    }

    auto requestContext = adoptNS([allocASCCredentialRequestContextInstance() initWithRequestTypes:requestTypes]);
    [requestContext setRelyingPartyIdentifier:options.rpId];
    if (mediation == MediationRequirement::Conditional) {
        if (![requestContext respondsToSelector:@selector(setRequestStyle:)])
            return nil;
        requestContext.get().requestStyle = ASCredentialRequestStyleAutoFill;
    }
    setGlobalFrameIDForContext(requestContext, globalFrameID);

    if (requestTypes & ASCCredentialRequestTypePlatformPublicKeyAssertion) {
        auto assertionOptions = configureAssertionOptions(options, hash, ASCPublicKeyCredentialKindPlatform, parentOrigin, allowedCredentials, userVerification);
        if (!assertionOptions)
            return nil;
        [requestContext setPlatformKeyCredentialAssertionOptions:assertionOptions.get()];
    }

    if (requestTypes & ASCCredentialRequestTypeSecurityKeyPublicKeyAssertion) {
        auto assertionOptions = configureAssertionOptions(options, hash, ASCPublicKeyCredentialKindSecurityKey, parentOrigin, allowedCredentials, userVerification);
        if (!assertionOptions)
            return nil;
        [requestContext setSecurityKeyCredentialAssertionOptions:assertionOptions.get()];
    }

    return requestContext;
}

static Vector<WebCore::AuthenticatorTransport> toAuthenticatorTransports(NSArray<NSNumber *> *ascTransports)
{
    Vector<WebCore::AuthenticatorTransport> transports;
    transports.reserveInitialCapacity(ascTransports.count);
    for (NSNumber *ascTransport : ascTransports) {
        if (WTF::isValidEnum<WebCore::AuthenticatorTransport>(ascTransport.intValue))
            transports.append(static_cast<WebCore::AuthenticatorTransport>(ascTransport.intValue));
    }
    return transports;
}

static std::optional<AuthenticationExtensionsClientOutputs> toExtensionOutputs(NSData *extensionOutputsCBOR)
{
    if (!extensionOutputsCBOR)
        return std::nullopt;
    return AuthenticationExtensionsClientOutputs::fromCBOR(vectorFromNSData(extensionOutputsCBOR));
}

bool WebAuthenticatorCoordinatorProxy::isASCAvailable()
{
    return isAuthenticationServicesCoreFrameworkAvailable();
}

RetainPtr<ASCCredentialRequestContext> WebAuthenticatorCoordinatorProxy::contextForRequest(WebAuthenticationRequestData&& requestData)
{
    RetainPtr<ASCCredentialRequestContext> result;
    WTF::switchOn(requestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        result = configureRegistrationRequestContext(options, requestData.hash, requestData.globalFrameID, requestData.mediation);
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        result = configurationAssertionRequestContext(options, requestData.hash, requestData.mediation, requestData.globalFrameID, requestData.parentOrigin);
    });
    return result;
}

static inline void continueAfterRequest(RetainPtr<id <ASCCredentialProtocol>> credential, RetainPtr<NSError> error, RequestCompletionHandler&& handler)
{
    AuthenticatorResponseData response = { };
    ExceptionData exceptionData = { };
    NSString *rawAttachment = nil;

    if ([credential isKindOfClass:getASCPlatformPublicKeyCredentialRegistrationClass()]) {
        response.isAuthenticatorAttestationResponse = true;

        ASCPlatformPublicKeyCredentialRegistration *registrationCredential = credential.get();
        response.rawId = toArrayBuffer(registrationCredential.credentialID);
        response.attestationObject = toArrayBuffer(registrationCredential.attestationObject);
        rawAttachment = registrationCredential.attachment;
        if ([registrationCredential respondsToSelector:@selector(transports)])
            response.transports = toAuthenticatorTransports(registrationCredential.transports);
        if ([registrationCredential respondsToSelector:@selector(extensionOutputsCBOR)])
            response.extensionOutputs = toExtensionOutputs(registrationCredential.extensionOutputsCBOR);
    } else if ([credential isKindOfClass:getASCSecurityKeyPublicKeyCredentialRegistrationClass()]) {
        response.isAuthenticatorAttestationResponse = true;

        ASCSecurityKeyPublicKeyCredentialRegistration *registrationCredential = credential.get();
        response.rawId = toArrayBuffer(registrationCredential.credentialID);
        response.attestationObject = toArrayBuffer(registrationCredential.attestationObject);
        rawAttachment = registrationCredential.attachment;
        if ([registrationCredential respondsToSelector:@selector(transports)])
            response.transports = toAuthenticatorTransports(registrationCredential.transports);
        if ([registrationCredential respondsToSelector:@selector(extensionOutputsCBOR)])
            response.extensionOutputs = toExtensionOutputs(registrationCredential.extensionOutputsCBOR);
    } else if ([credential isKindOfClass:getASCPlatformPublicKeyCredentialAssertionClass()]) {
        response.isAuthenticatorAttestationResponse = false;

        ASCPlatformPublicKeyCredentialAssertion *assertionCredential = credential.get();
        response.rawId = toArrayBuffer(assertionCredential.credentialID);
        response.authenticatorData = toArrayBuffer(assertionCredential.authenticatorData);
        response.signature = toArrayBuffer(assertionCredential.signature);
        response.userHandle = toArrayBuffer(assertionCredential.userHandle);
        rawAttachment = assertionCredential.attachment;
        if ([assertionCredential respondsToSelector:@selector(extensionOutputsCBOR)])
            response.extensionOutputs = toExtensionOutputs(assertionCredential.extensionOutputsCBOR);
    } else if ([credential isKindOfClass:getASCSecurityKeyPublicKeyCredentialAssertionClass()]) {
        response.isAuthenticatorAttestationResponse = false;

        ASCSecurityKeyPublicKeyCredentialAssertion *assertionCredential = credential.get();
        response.rawId = toArrayBuffer(assertionCredential.credentialID);
        response.authenticatorData = toArrayBuffer(assertionCredential.authenticatorData);
        response.signature = toArrayBuffer(assertionCredential.signature);
        response.userHandle = toArrayBuffer(assertionCredential.userHandle);
        rawAttachment = assertionCredential.attachment;
        if ([assertionCredential respondsToSelector:@selector(extensionOutputsCBOR)])
            response.extensionOutputs = toExtensionOutputs(assertionCredential.extensionOutputsCBOR);
    } else {
        ExceptionCode exceptionCode;
        NSString *errorMessage = nil;
        if ([error.get().domain isEqualToString:WKErrorDomain]) {
            exceptionCode = toExceptionCode(error.get().code);
            errorMessage = error.get().userInfo[NSLocalizedDescriptionKey];
        } else {
            exceptionCode = ExceptionCode::NotAllowedError;

            if ([error.get().domain isEqualToString:ASCAuthorizationErrorDomain] && error.get().code == ASCAuthorizationErrorUserCanceled) {
                errorMessage = @"This request has been cancelled by the user.";
                RELEASE_LOG_ERROR(WebAuthn, "Request cancelled after ASCAuthorizationErrorUserCanceled.");
            } else {
                errorMessage = @"Operation failed.";
                RELEASE_LOG_ERROR(WebAuthn, "Request cancelled after error: %@.", error.get().localizedDescription);
            }
        }

        exceptionData = { exceptionCode, errorMessage };
    }
    
    AuthenticatorAttachment attachment;
    if ([rawAttachment isEqualToString:@"platform"])
        attachment = AuthenticatorAttachment::Platform;
    else
        attachment = AuthenticatorAttachment::CrossPlatform;

    handler(response, attachment, exceptionData);
}

void WebAuthenticatorCoordinatorProxy::performRequestLegacy(RetainPtr<ASCCredentialRequestContext> requestContext, RequestCompletionHandler&& handler)
{
    if (requestContext.get().requestTypes == ASCCredentialRequestTypeNone) {
        handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "This request has been cancelled by the user."_s });
        RELEASE_LOG_ERROR(WebAuthn, "Request cancelled due to none requestTypes.");
        return;
    }
    m_proxy = adoptNS([allocASCAgentProxyInstance() init]);

    if (requestContext.get().requestStyle == ASCredentialRequestStyleSilent) {
        [m_proxy performSilentAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler)](id<ASCCredentialProtocol> credential, NSError *error) mutable {
            ensureOnMainRunLoop([weakThis, handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
                if (!weakThis) {
                    handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "Operation failed."_s });
                    RELEASE_LOG_ERROR(WebAuthn, "Request cancelled after WebAuthenticatorCoordinatorProxy invalid after starting request.");
                    return;
                }

                continueAfterRequest(credential, error, WTFMove(handler));
                if (weakThis->m_proxy)
                    weakThis->m_proxy.clear();
            });
        }).get()];
        return;
    }

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if ([requestContext respondsToSelector:@selector(requestStyle)] && requestContext.get().requestStyle == ASCredentialRequestStyleAutoFill) {
        [m_proxy performAutoFillAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler)](id<ASCCredentialProtocol> credential, NSError *error) mutable {
            ensureOnMainRunLoop([weakThis, handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
                if (!weakThis) {
                    handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "Operation failed."_s });
                    RELEASE_LOG_ERROR(WebAuthn, "Request cancelled after WebAuthenticatorCoordinatorProxy invalid after staring request.");
                    return;
                }
                continueAfterRequest(credential, error, WTFMove(handler));
                if (weakThis->m_proxy)
                    weakThis->m_proxy.clear();
            });
        }).get()];
        return;
    }
#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)
#if PLATFORM(IOS) || PLATFORM(VISION)
    requestContext.get().windowSceneIdentifier = m_webPageProxy.pageClient().sceneID();

    [m_proxy performAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler)](id<ASCCredentialProtocol> credential, NSError *error) mutable {
        callOnMainRunLoop([weakThis, handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
#elif PLATFORM(MAC)
    RetainPtr<NSWindow> window = m_webPageProxy.platformWindow();
    [m_proxy performAuthorizationRequestsForContext:requestContext.get() withClearanceHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler), window = WTFMove(window)](NSXPCListenerEndpoint *daemonEndpoint, NSError *error) mutable {
        callOnMainRunLoop([weakThis, handler = WTFMove(handler), window = WTFMove(window), daemonEndpoint = retainPtr(daemonEndpoint), error = retainPtr(error)] () mutable {
            if (!weakThis || !daemonEndpoint) {
                RELEASE_LOG_ERROR(WebAuthn, "Could not connect to authorization daemon: %@.", error.get().localizedDescription);
                handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "Operation failed."_s });
                if (weakThis && weakThis->m_proxy)
                    weakThis->m_proxy.clear();
                return;
            }

            weakThis->m_presenter = adoptNS([allocASCAuthorizationRemotePresenterInstance() init]);
            [weakThis->m_presenter presentWithWindow:window.get() daemonEndpoint:daemonEndpoint.get() completionHandler:makeBlockPtr([weakThis, handler = WTFMove(handler)](id<ASCCredentialProtocol> credentialNotRetain, NSError *errorNotRetain) mutable {
                auto credential = retainPtr(credentialNotRetain);
                auto error = retainPtr(errorNotRetain);
#endif
                continueAfterRequest(credential, error, WTFMove(handler));
                if (weakThis && weakThis->m_proxy)
                    weakThis->m_proxy.clear();
#if PLATFORM(MAC)
            }).get()];
#endif
        });
    }).get()];
}

static inline bool canCurrentProcessAccessPasskeysForRelyingParty(const WebCore::SecurityOriginData& data)
{
    if ([getASCWebKitSPISupportClass() respondsToSelector:@selector(canCurrentProcessAccessPasskeysForRelyingParty:)])
        return [getASCWebKitSPISupportClass() canCurrentProcessAccessPasskeysForRelyingParty:data.securityOrigin()->domain()];
    return false;
}

void WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(const WebCore::SecurityOriginData& data, QueryCompletionHandler&& handler)
{
    if (canCurrentProcessAccessPasskeysForRelyingParty(data)) {
        handler([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]);
        return;
    }
    handler(false);
}

void WebAuthenticatorCoordinatorProxy::getClientCapabilities(const WebCore::SecurityOriginData& originData, CapabilitiesCompletionHandler&& handler)
{
    if (![getASCWebKitSPISupportClass() respondsToSelector:@selector(getClientCapabilitiesForRelyingParty:withCompletionHandler:)]) {
        Vector<KeyValuePair<String, bool>> capabilities;
        handler(WTFMove(capabilities));
        return;
    }

    [getASCWebKitSPISupportClass() getClientCapabilitiesForRelyingParty:originData.securityOrigin()->domain() withCompletionHandler:makeBlockPtr([handler = WTFMove(handler)](NSDictionary<NSString *, NSNumber *> *result) mutable {
        Vector<KeyValuePair<String, bool>> capabilities;
        for (NSString *key in result)
            capabilities.append({ key, result[key].boolValue });

        ensureOnMainRunLoop([handler = WTFMove(handler), capabilities = WTFMove(capabilities)] () mutable {
            handler(WTFMove(capabilities));
        });
    }).get()];
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOriginData& data, QueryCompletionHandler&& handler)
{
    if (canCurrentProcessAccessPasskeysForRelyingParty(data)) {
        if ([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]) {
            handler(![getASCWebKitSPISupportClass() respondsToSelector:@selector(arePasskeysDisallowedForRelyingParty:)] || ![getASCWebKitSPISupportClass() arePasskeysDisallowedForRelyingParty:data.securityOrigin()->domain()]);
            return;
        }
        handler(LocalService::isAvailable());
        return;
    }
    handler(false);
}

void WebAuthenticatorCoordinatorProxy::cancel()
{
    if (m_proxy) {
        [m_proxy cancelCurrentRequest];
        m_proxy.clear();
    }
#if HAVE(WEB_AUTHN_AS_MODERN)
    if (m_controller) {
        [m_controller cancel];
        m_controller.clear();
        m_delegate.clear();
        m_completionHandler = nullptr;
    }
#endif
}

} // namespace WebKit

#endif // HAVE(UNIFIED_ASC_AUTH_UI)
