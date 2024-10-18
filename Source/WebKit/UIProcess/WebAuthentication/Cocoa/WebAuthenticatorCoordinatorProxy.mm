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

#include <wtf/Assertions.h>
#if HAVE(UNIFIED_ASC_AUTH_UI) || HAVE(WEB_AUTHN_AS_MODERN)

#import "config.h"
#import "WebAuthenticatorCoordinatorProxy.h"

#import "ArgumentCoders.h"
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
#import <WebCore/AuthenticatorSelectionCriteria.h>
#import <WebCore/AuthenticatorTransport.h>
#import <WebCore/BufferSource.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <WebCore/PublicKeyCredentialParameters.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/WebAuthenticationUtils.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>
#import "AuthenticationServicesCoreSoftLink.h"
#if HAVE(WEB_AUTHN_AS_MODERN)
#import "AuthenticationServicesSoftLink.h"

@interface _WKASDelegate : NSObject {
    RetainPtr<WKWebView> m_view;
    BlockPtr<void(ASAuthorization *, NSError *)> m_completionHandler;
}
- (instancetype)initWithPage:(WeakPtr<WebKit::WebPageProxy> &&)page completionHandler:(BlockPtr<void(ASAuthorization *, NSError *)> &&)completionHandler;
@end

@implementation _WKASDelegate
- (instancetype)initWithPage:(WeakPtr<WebKit::WebPageProxy> &&)page completionHandler:(BlockPtr<void(ASAuthorization *, NSError *)> &&)completionHandler
{
    if (!(self = [super init]))
        return nil;

    if (page)
        m_view = page->cocoaView();
    m_completionHandler = WTFMove(completionHandler);

    return self;
}

- (ASPresentationAnchor)presentationAnchorForAuthorizationController:(ASAuthorizationController *)controller
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (m_view)
        return [m_view window];
    return nil;
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
    return ArrayBuffer::create(span(data));
}

static inline RefPtr<ArrayBuffer> toArrayBufferNilIfEmpty(NSData *data)
{
    if (!data || !data.length)
        return nullptr;
    return ArrayBuffer::create(span(data));
}

static inline ExceptionCode toExceptionCode(NSInteger nsErrorCode)
{
    ExceptionCode exceptionCode = (ExceptionCode)nsErrorCode;

    switch (exceptionCode) {
    case ExceptionCode::IndexSizeError:
    case ExceptionCode::HierarchyRequestError:
    case ExceptionCode::WrongDocumentError:
    case ExceptionCode::InvalidCharacterError:
    case ExceptionCode::NoModificationAllowedError:
    case ExceptionCode::NotFoundError:
    case ExceptionCode::NotSupportedError:
    case ExceptionCode::InUseAttributeError:
    case ExceptionCode::InvalidStateError:
    case ExceptionCode::SyntaxError:
    case ExceptionCode::InvalidModificationError:
    case ExceptionCode::NamespaceError:
    case ExceptionCode::InvalidAccessError:
    case ExceptionCode::TypeMismatchError:
    case ExceptionCode::SecurityError:
    case ExceptionCode::NetworkError:
    case ExceptionCode::AbortError:
    case ExceptionCode::URLMismatchError:
    case ExceptionCode::QuotaExceededError:
    case ExceptionCode::TimeoutError:
    case ExceptionCode::InvalidNodeTypeError:
    case ExceptionCode::DataCloneError:
    case ExceptionCode::EncodingError:
    case ExceptionCode::NotReadableError:
    case ExceptionCode::UnknownError:
    case ExceptionCode::ConstraintError:
    case ExceptionCode::DataError:
    case ExceptionCode::TransactionInactiveError:
    case ExceptionCode::ReadonlyError:
    case ExceptionCode::VersionError:
    case ExceptionCode::OperationError:
    case ExceptionCode::NotAllowedError:
    case ExceptionCode::RangeError:
    case ExceptionCode::TypeError:
    case ExceptionCode::JSSyntaxError:
    case ExceptionCode::StackOverflowError:
    case ExceptionCode::OutOfMemoryError:
    case ExceptionCode::ExistingExceptionError:
        return exceptionCode;
    }

    return ExceptionCode::NotAllowedError;
}

#if HAVE(WEB_AUTHN_AS_MODERN)

static inline ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement toASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement(const String& requirement)
{
    if (requirement == "required"_s)
        return ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementRequired;
    return ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementPreferred;
}


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

#if HAVE(SECURITY_KEY_API)
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
#endif // HAVE(SECURITY_KEY_API)

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

RetainPtr<ASAuthorizationController> WebAuthenticatorCoordinatorProxy::constructASController(const WebAuthenticationRequestData& requestData)
{
    RetainPtr<NSArray> requests;
    WTF::switchOn(requestData.options, [&](const PublicKeyCredentialCreationOptions &options) {
        requests = requestsForRegistration(options, requestData.frameInfo.securityOrigin);
    }, [&](const PublicKeyCredentialRequestOptions &options) {
        requests = requestsForAssertion(options, requestData.frameInfo.securityOrigin, requestData.parentOrigin);
    });
    if (!requests || ![requests count])
        return nullptr;
    return adoptNS([allocASAuthorizationControllerInstance() initWithAuthorizationRequests:requests.get()]);
}

#if HAVE(WEB_AUTHN_PRF_API)
static inline RetainPtr<ASAuthorizationPublicKeyCredentialPRFAssertionInputValues> toASAssertionPRFInputValue(std::optional<WebCore::AuthenticationExtensionsClientInputs::PRFValues> eval)
{
    if (!eval)
        return nullptr;

    auto first = WebCore::toNSData(eval->first);

    RetainPtr<NSData> second;
    if (eval->second)
        second = WebCore::toNSData(*eval->second);

    if (eval->second)
        return adoptNS([WebKit::allocASAuthorizationPublicKeyCredentialPRFAssertionInputValuesInstance() initWithSaltInput1:first.get() saltInput2:WebCore::toNSData(*eval->second).get()]);
    return adoptNS([WebKit::allocASAuthorizationPublicKeyCredentialPRFAssertionInputValuesInstance() initWithSaltInput1:first.get() saltInput2:nil]);
}
#endif

RetainPtr<NSArray> WebAuthenticatorCoordinatorProxy::requestsForRegistration(const PublicKeyCredentialCreationOptions &options, const WebCore::SecurityOriginData& callerOrigin)
{
    RetainPtr<NSMutableArray<ASAuthorizationRequest *>> requests = adoptNS([[NSMutableArray alloc] init]);
    bool includePlatformRequest = true;

#if HAVE(SECURITY_KEY_API)
    bool includeSecurityKeyRequest = true;
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
#endif // HAVE(SECURITY_KEY_API)

    RetainPtr<NSMutableArray<ASAuthorizationPlatformPublicKeyCredentialDescriptor *>> platformExcludedCredentials = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *>> crossPlatformExcludedCredentials = adoptNS([[NSMutableArray alloc] init]);
    for (auto credential : options.excludeCredentials) {
        if (credential.transports.contains(AuthenticatorTransport::Internal) || credential.transports.isEmpty())
            [platformExcludedCredentials addObject:adoptNS([allocASAuthorizationPlatformPublicKeyCredentialDescriptorInstance() initWithCredentialID:toNSData(credential.id).get()]).get()];
        if (credential.transports.isEmpty() || !credential.transports.contains(AuthenticatorTransport::Internal)) {
            RetainPtr<NSMutableArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport>> transports = adoptNS([[NSMutableArray alloc] init]);
            for (auto transport : credential.transports) {
                if (auto asTransport = toASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport(transport))
                    [transports addObject:asTransport.get()];
            }
            [crossPlatformExcludedCredentials addObject:adoptNS([allocASAuthorizationSecurityKeyPublicKeyCredentialDescriptorInstance() initWithCredentialID:toNSData(credential.id).get() transports:transports.get()]).get()];
        }
    }

    RetainPtr<ASPublicKeyCredentialClientData> clientData = adoptNS([allocASPublicKeyCredentialClientDataInstance() initWithChallenge:toNSData(options.challenge).get() origin:callerOrigin.toString()]);
    if (includePlatformRequest) {
        RetainPtr request = adoptNS([[allocASAuthorizationPlatformPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:options.rp.id] createCredentialRegistrationRequestWithClientData:clientData.get() name:options.user.name userID:toNSData(options.user.id).get()]);

        if (m_isConditionalMediation && [request respondsToSelector:@selector(setRequestStyle:)])
            request.get().requestStyle = ASAuthorizationPlatformPublicKeyCredentialRegistrationRequestStyleConditional;
#if HAVE(WEB_AUTHN_PRF_API)
        if (options.extensions && options.extensions->prf) {
            auto prf = options.extensions->prf;
            if (prf->eval)
                request.get().prf = adoptNS([allocASAuthorizationPublicKeyCredentialPRFRegistrationInputInstance() initWithInputValues:toASAssertionPRFInputValue(prf->eval).get()]).get();
            else
                request.get().prf = [getASAuthorizationPublicKeyCredentialPRFRegistrationInputClass() checkForSupport];
        }
#endif

        // Platform credentials may only support enterprise attestation.
        if (options.attestation == AttestationConveyancePreference::Enterprise)
            request.get().attestationPreference = toAttestationConveyancePreference(options.attestation).get();
        if (options.authenticatorSelection)
            request.get().userVerificationPreference = toASUserVerificationPreference(options.authenticatorSelection->userVerification).get();
        if (options.extensions && options.extensions->largeBlob) {
            // These are satisfied by validation in AuthenticatorCoordinator.
            ASSERT(!options.extensions->largeBlob->read && !options.extensions->largeBlob->write);
            request.get().largeBlob = adoptNS([allocASAuthorizationPublicKeyCredentialLargeBlobRegistrationInputInstance() initWithSupportRequirement:toASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement(options.extensions->largeBlob->support)]).get();
        }
        request.get().excludedCredentials = platformExcludedCredentials.get();
        [requests addObject:request.leakRef()];
    }
#if HAVE(SECURITY_KEY_API)
    if (includeSecurityKeyRequest) {
        RetainPtr provider = adoptNS([allocASAuthorizationSecurityKeyPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:options.rp.id]);
        RetainPtr<ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest> request;
        if ([provider respondsToSelector:@selector(createCredentialRegistrationRequestWithClientData:displayName:name:userID:)])
            request = adoptNS([provider createCredentialRegistrationRequestWithClientData:clientData.get() displayName:options.user.displayName name:options.user.name userID:toNSData(options.user.id).get()]);
        else
            request = adoptNS([provider createCredentialRegistrationRequestWithChallenge:toNSData(options.challenge).get() displayName:options.user.displayName name:options.user.name userID:toNSData(options.user.id).get()]);
        request.get().attestationPreference = toAttestationConveyancePreference(options.attestation).get();
        RetainPtr<NSMutableArray<ASAuthorizationPublicKeyCredentialParameters *>> parameters = adoptNS([[NSMutableArray alloc] init]);
        for (auto alg : options.pubKeyCredParams)
            [parameters addObject:adoptNS([allocASAuthorizationPublicKeyCredentialParametersInstance() initWithAlgorithm:alg.alg]).get()];
        request.get().credentialParameters = parameters.get();
        if (options.authenticatorSelection) {
            request.get().userVerificationPreference = toASUserVerificationPreference(options.authenticatorSelection->userVerification).get();
            request.get().residentKeyPreference = toASResidentKeyPreference(options.authenticatorSelection->residentKey, options.authenticatorSelection->requireResidentKey).get();
        }
        request.get().excludedCredentials = crossPlatformExcludedCredentials.get();
        [requests addObject:request.leakRef()];
    }
#endif // HAVE(SECURITY_KEY_API)

    return requests;
}

static inline bool isPlatformRequest(const Vector<AuthenticatorTransport>& transports)
{
    return transports.isEmpty() || transports.containsIf([](auto transport) {
        return transport == AuthenticatorTransport::Internal || transport == AuthenticatorTransport::Hybrid;
    });
}

static inline bool isCrossPlatformRequest(const Vector<AuthenticatorTransport>& transports)
{
    return transports.isEmpty() || transports.containsIf([](auto transport) {
        return transport != AuthenticatorTransport::Internal && transport != AuthenticatorTransport::Hybrid;
    });
}

static inline bool allowsHybrid(const Vector<AuthenticatorTransport>& transports)
{
    return transports.isEmpty() || transports.containsIf([](auto transport) {
        return transport == AuthenticatorTransport::Hybrid || transport == AuthenticatorTransport::Cable;
    });
}

RetainPtr<NSArray> WebAuthenticatorCoordinatorProxy::requestsForAssertion(const PublicKeyCredentialRequestOptions &options, const WebCore::SecurityOriginData& callerOrigin, const std::optional<WebCore::SecurityOriginData>& parentOrigin)
{
    RetainPtr<NSMutableArray<ASAuthorizationRequest *>> requests = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<ASAuthorizationPlatformPublicKeyCredentialDescriptor *>> platformAllowedCredentials = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr<NSMutableArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *>> crossPlatformAllowedCredentials = adoptNS([[NSMutableArray alloc] init]);
    bool allowHybrid = options.allowCredentials.isEmpty();
    for (auto credential : options.allowCredentials) {
        if (isPlatformRequest(credential.transports))
            [platformAllowedCredentials addObject:adoptNS([allocASAuthorizationPlatformPublicKeyCredentialDescriptorInstance() initWithCredentialID:toNSData(credential.id).get()]).get()];
        if (isCrossPlatformRequest(credential.transports)) {
            RetainPtr<NSMutableArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport>> transports = adoptNS([[NSMutableArray alloc] init]);
            for (auto transport : credential.transports) {
                if (auto asTransport = toASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport(transport))
                    [transports addObject:asTransport.get()];
            }
            [crossPlatformAllowedCredentials addObject:adoptNS([allocASAuthorizationSecurityKeyPublicKeyCredentialDescriptorInstance() initWithCredentialID:toNSData(credential.id).get() transports:transports.get()]).get()];
        }

        if (!allowHybrid && allowsHybrid(credential.transports))
            allowHybrid = true;
    }
    RetainPtr clientData = adoptNS([allocASPublicKeyCredentialClientDataInstance() initWithChallenge:toNSData(options.challenge).get() origin:callerOrigin.toString()]);
    if (parentOrigin) {
        clientData.get().crossOrigin = ASPublicKeyCredentialClientDataCrossOriginValueCrossOrigin;
        clientData.get().topOrigin = parentOrigin->toString();
    }
    if ([platformAllowedCredentials count] || ![crossPlatformAllowedCredentials count]) {
        RetainPtr request = adoptNS([[allocASAuthorizationPlatformPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:options.rpId] createCredentialAssertionRequestWithClientData:clientData.get()]);
        if (platformAllowedCredentials)
            request.get().allowedCredentials = platformAllowedCredentials.get();
        if (options.extensions && options.extensions->largeBlob) {
            // These are satisfied by validation in AuthenticatorCoordinator.
            ASSERT(!options.extensions->largeBlob->support);
            ASSERT(!(options.extensions->largeBlob->read && options.extensions->largeBlob->write));
            auto largeBlob = options.extensions->largeBlob;
            RetainPtr asLargeBlob = adoptNS([allocASAuthorizationPublicKeyCredentialLargeBlobAssertionInputInstance() initWithOperation:(largeBlob->read && *largeBlob->read) ? ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationRead : ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationWrite]);
            if (largeBlob->write)
                asLargeBlob.get().dataToWrite = WebCore::toNSData(*largeBlob->write).get();
            request.get().largeBlob = asLargeBlob.get();
        }

        request.get().userVerificationPreference = toASUserVerificationPreference(options.userVerification).get();

        if (!allowHybrid)
            request.get().shouldShowHybridTransport = false;

#if HAVE(WEB_AUTHN_PRF_API)
        if (options.extensions && options.extensions->prf) {
            auto prf = options.extensions->prf;
            RetainPtr inputValues = toASAssertionPRFInputValue(prf->eval);
            RetainPtr<NSMutableDictionary> perCredentialInputValues = nullptr;
            if (prf->evalByCredential) {
                perCredentialInputValues = adoptNS([[NSMutableDictionary alloc] init]);
                for (auto& credentialIDAndInputValues : *prf->evalByCredential) {
                    auto key = base64URLDecode(credentialIDAndInputValues.key.utf8().span());
                    if (!key)
                        continue;
                    [perCredentialInputValues setObject:toASAssertionPRFInputValue(credentialIDAndInputValues.value).get() forKey: toNSData(*key).get()];
                }
            }
            request.get().prf = adoptNS([allocASAuthorizationPublicKeyCredentialPRFAssertionInputInstance() initWithInputValues:inputValues.get() perCredentialInputValues:perCredentialInputValues.get()]).get();
        }
#endif

        [requests addObject:request.leakRef()];
    }

#if HAVE(SECURITY_KEY_API)
    if (!m_isConditionalMediation && ([crossPlatformAllowedCredentials count] || ![platformAllowedCredentials count])) {
        RetainPtr provider = adoptNS([allocASAuthorizationSecurityKeyPublicKeyCredentialProviderInstance() initWithRelyingPartyIdentifier:options.rpId]);
        RetainPtr<ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest> request;
        if ([provider respondsToSelector:@selector(createCredentialAssertionRequestWithClientData:)])
            request = adoptNS([provider createCredentialAssertionRequestWithClientData:clientData.get()]);
        else
            request = adoptNS([provider createCredentialAssertionRequestWithChallenge:toNSData(options.challenge).get()]);

        if (crossPlatformAllowedCredentials)
            request.get().allowedCredentials = crossPlatformAllowedCredentials.get();
        if (options.extensions && !options.extensions->appid.isNull())
            request.get().appID = options.extensions->appid;
        [requests addObject:request.leakRef()];
    }
#endif // HAVE(SECURITY_KEY_API)

    return requests;
}

WeakPtr<WebAuthenticatorCoordinatorProxy>& WebAuthenticatorCoordinatorProxy::activeConditionalMediationProxy()
{
    static MainThreadNeverDestroyed<WeakPtr<WebAuthenticatorCoordinatorProxy>> proxy;
    return proxy.get();
}

void WebAuthenticatorCoordinatorProxy::pauseConditionalAssertion(CompletionHandler<void()>&& completionHandler)
{
    if (m_paused) {
        completionHandler();
        return;
    }

    if (m_isConditionalMediation) {
        m_paused = true;
        m_cancelHandler = WTFMove(completionHandler);
        [m_controller cancel];
    } else
        completionHandler();
}

void WebAuthenticatorCoordinatorProxy::unpauseConditionalAssertion()
{
    if (!m_paused)
        return;
    if (m_controller && m_isConditionalMediation) {
        activeConditionalMediationProxy() = *this;
        [m_controller performAutoFillAssistedRequests];
    }

    m_paused = false;
}

void WebAuthenticatorCoordinatorProxy::makeActiveConditionalAssertion()
{
    if (auto& activeProxy = activeConditionalMediationProxy()) {
        if (activeProxy == this && !m_paused)
            return;
        activeProxy->pauseConditionalAssertion([weakThis = WeakPtr { *this }] () {
            if (!weakThis)
                return;
            weakThis->unpauseConditionalAssertion();
        });
        return;
    }
    unpauseConditionalAssertion();
}

inline static Vector<AuthenticatorTransport> toTransports(NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *asTransports)
{
    Vector<AuthenticatorTransport> transports;
    for (ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport asTransport : asTransports) {
        if (auto transport = convertStringToAuthenticatorTransport(asTransport))
            transports.append(*transport);
    }
    return transports;
}

#endif // HAVE(WEB_AUTHN_AS_MODERN)

void WebAuthenticatorCoordinatorProxy::performRequest(WebAuthenticationRequestData &&requestData, RequestCompletionHandler &&handler)
{
#if HAVE(UNIFIED_ASC_AUTH_UI)
    RefPtr webPageProxy = m_webPageProxy.get();
    if (!webPageProxy || !webPageProxy->protectedPreferences()->webAuthenticationASEnabled()) {
        auto context = contextForRequest(WTFMove(requestData));
        if (context.get() == nullptr) {
            handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
            RELEASE_LOG_ERROR(WebAuthn, "The origin of the document is not the same as its ancestors.");
            return;
        }
        performRequestLegacy(context, WTFMove(handler));
        return;
    }
#endif

#if HAVE(WEB_AUTHN_AS_MODERN)
    m_isConditionalMediation = requestData.mediation == MediationRequirement::Conditional;
    auto controller = constructASController(requestData);
    if (!controller) {
        handler(WebCore::AuthenticatorResponseData { }, AuthenticatorAttachment::Platform, { ExceptionCode::NotAllowedError, @"" });
        return;
    }
    if (m_isConditionalMediation)
        activeConditionalMediationProxy() = *this;
    m_controller = WTFMove(controller);
    m_completionHandler = WTFMove(handler);
    m_delegate = adoptNS([[_WKASDelegate alloc] initWithPage:WTFMove(requestData.page) completionHandler:makeBlockPtr([weakThis = WeakPtr { *this }](ASAuthorization *auth, NSError *error) mutable {
        ensureOnMainRunLoop([weakThis = WTFMove(weakThis), auth = retainPtr(auth), error = retainPtr(error)]() {
            if (!weakThis)
                return;
            WebCore::AuthenticatorResponseData response = { };
            WebCore::ExceptionData exceptionData = { ExceptionCode::NotAllowedError, @"" };
            WebCore::AuthenticatorAttachment attachment = AuthenticatorAttachment::Platform;
            if ([error.get().domain isEqualToString:WKErrorDomain])
                exceptionData = { toExceptionCode(error.get().code), error.get().userInfo[NSLocalizedDescriptionKey] };
            else if ([error.get().domain isEqualToString:ASAuthorizationErrorDomain]) {
                switch (error.get().code) {
                case ASAuthorizationErrorFailed: {
                    RetainPtr<NSError> underlyingError = retainPtr(error.get().userInfo[NSUnderlyingErrorKey]);
                    if ([underlyingError.get().domain isEqualToString:ASCAuthorizationErrorDomain] && underlyingError.get().code == ASCAuthorizationErrorSecurityError)
                        exceptionData = { ExceptionCode::SecurityError, underlyingError.get().userInfo[NSLocalizedFailureReasonErrorKey] };
                    else if ([underlyingError.get().domain isEqualToString:WKErrorDomain])
                        exceptionData = { toExceptionCode(underlyingError.get().code), underlyingError.get().userInfo[NSLocalizedDescriptionKey] };
                    break;
                }
                case ASAuthorizationErrorMatchedExcludedCredential:
                    exceptionData = { ExceptionCode::InvalidStateError, @"" };
                    break;
                case ASAuthorizationErrorCanceled:
                    exceptionData = { ExceptionCode::NotAllowedError, @"" };
                    break;
                case ASAuthorizationErrorUnknown:
                case ASAuthorizationErrorInvalidResponse:
                case ASAuthorizationErrorNotHandled:
                case ASAuthorizationErrorNotInteractive:
                    ASSERT_NOT_REACHED();
                    break;
                }
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationPlatformPublicKeyCredentialRegistrationClass()]) {
                response.isAuthenticatorAttestationResponse = true;
                auto credential = retainPtr((ASAuthorizationPlatformPublicKeyCredentialRegistration *)auth.get().credential);
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.attestationObject = toArrayBuffer(credential.get().rawAttestationObject);
                response.transports = { AuthenticatorTransport::Internal, AuthenticatorTransport::Hybrid };
                response.clientDataJSON = toArrayBuffer(credential.get().rawClientDataJSON);

                bool hasExtensionOutput = false;
                AuthenticationExtensionsClientOutputs extensionOutputs;
                if (credential.get().largeBlob) {
                    hasExtensionOutput = true;
                    extensionOutputs.largeBlob = { credential.get().largeBlob.isSupported, nullptr, std::nullopt };
                }

#if HAVE(WEB_AUTHN_PRF_API)
                if ([credential respondsToSelector:@selector(prf)] && credential.get().prf) {
                    hasExtensionOutput = true;
                    RefPtr<ArrayBuffer> first = nullptr;
                    RefPtr<ArrayBuffer> second = nullptr;
                    if (credential.get().prf.first)
                        first = toArrayBuffer(credential.get().prf.first);
                    if (credential.get().prf.second)
                        second = toArrayBuffer(credential.get().prf.second);
                    if (first)
                        extensionOutputs.prf = { credential.get().prf.isSupported, { { first, second } } };
                    else
                        extensionOutputs.prf = { credential.get().prf.isSupported, std::nullopt };
                }
#endif

                if (hasExtensionOutput)
                    response.extensionOutputs = extensionOutputs;
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationPlatformPublicKeyCredentialAssertionClass()]) {
                auto credential = retainPtr((ASAuthorizationPlatformPublicKeyCredentialAssertion *)auth.get().credential);
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.authenticatorData = toArrayBuffer(credential.get().rawAuthenticatorData);
                response.signature = toArrayBuffer(credential.get().signature);
                response.userHandle = toArrayBufferNilIfEmpty(credential.get().userID);
                response.clientDataJSON = toArrayBuffer(credential.get().rawClientDataJSON);

                bool hasExtensionOutput = false;
                AuthenticationExtensionsClientOutputs extensionOutputs;
                if (credential.get().largeBlob) {
                    hasExtensionOutput = true;
                    RefPtr<ArrayBuffer> protector = nullptr;
                    if (credential.get().largeBlob.readData)
                        protector = toArrayBuffer(credential.get().largeBlob.readData);
                    extensionOutputs.largeBlob = { std::nullopt, protector, credential.get().largeBlob.didWrite };
                }

#if HAVE(WEB_AUTHN_PRF_API)
                if ([credential respondsToSelector:@selector(prf)] && credential.get().prf) {
                    hasExtensionOutput = true;
                    RefPtr<ArrayBuffer> first = toArrayBuffer(credential.get().prf.first);
                    RefPtr<ArrayBuffer> second = nullptr;
                    if (credential.get().prf.second)
                        second = toArrayBuffer(credential.get().prf.second);
                    extensionOutputs.prf = { std::nullopt, { { first, second } } };
                }
#endif

                if (hasExtensionOutput)
                    response.extensionOutputs = extensionOutputs;
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationSecurityKeyPublicKeyCredentialRegistrationClass()]) {
                auto credential = retainPtr((ASAuthorizationSecurityKeyPublicKeyCredentialRegistration *)auth.get().credential);
                response.isAuthenticatorAttestationResponse = true;
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.attestationObject = toArrayBuffer(credential.get().rawAttestationObject);
                if ([credential respondsToSelector:@selector(transports)])
                    response.transports = toTransports(credential.get().transports);
                else
                    response.transports = { };
                response.clientDataJSON = toArrayBuffer(credential.get().rawClientDataJSON);
            } else if ([auth.get().credential isKindOfClass:getASAuthorizationSecurityKeyPublicKeyCredentialAssertionClass()]) {
                auto credential = retainPtr((ASAuthorizationSecurityKeyPublicKeyCredentialAssertion *)auth.get().credential);
                response.rawId = toArrayBuffer(credential.get().credentialID);
                response.authenticatorData = toArrayBuffer(credential.get().rawAuthenticatorData);
                response.signature = toArrayBuffer(credential.get().signature);
                response.userHandle = toArrayBufferNilIfEmpty(credential.get().userID);
                response.clientDataJSON = toArrayBuffer(credential.get().rawClientDataJSON);
                if ([credential respondsToSelector:@selector(appID)]) {
                    AuthenticationExtensionsClientOutputs extensionOutputs;
                    extensionOutputs.appid = credential.get().appID;
                    response.extensionOutputs = extensionOutputs;
                }
            }

            if (weakThis) {
                if (activeConditionalMediationProxy() == weakThis)
                    activeConditionalMediationProxy() = nullptr;
                if (!weakThis->m_paused) {
                    weakThis->m_completionHandler(response, attachment, exceptionData);
                    weakThis->m_delegate.clear();
                    weakThis->m_controller.clear();
                    weakThis->m_isConditionalMediation = false;
                }
                if (auto cancelHandler = WTFMove(weakThis->m_cancelHandler))
                    cancelHandler();
            }
        });
    }).get()]);

    m_controller.get().presentationContextProvider = (id<ASAuthorizationControllerPresentationContextProviding>)m_delegate.get();
    m_controller.get().delegate = (id<ASAuthorizationControllerDelegate>)m_delegate.get();
    if (requestData.mediation && *requestData.mediation == MediationRequirement::Conditional && std::holds_alternative<PublicKeyCredentialRequestOptions>(requestData.options))
        [m_controller performAutoFillAssistedRequests];
    else
        [m_controller performRequests];
#endif
}

#if HAVE(UNIFIED_ASC_AUTH_UI)
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

static RetainPtr<ASCCredentialRequestContext> configureRegistrationRequestContext(const PublicKeyCredentialCreationOptions& options, std::optional<WebCore::GlobalFrameIdentifier> globalFrameID, std::optional<WebCore::MediationRequirement> mediation, const WebCore::SecurityOriginData& callerOrigin)
{
    ASCCredentialRequestTypes requestTypes = ASCCredentialRequestTypePlatformPublicKeyRegistration | ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration;

    RetainPtr<NSString> userVerification;
    bool shouldRequireResidentKey = false;
    std::optional<ResidentKeyRequirement> residentKeyRequirement;
    std::optional<AuthenticatorSelectionCriteria> authenticatorSelection = options.authenticatorSelection;
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
    [requestContext setRelyingPartyIdentifier:options.rp.id];
    setGlobalFrameIDForContext(requestContext, globalFrameID);

    auto credentialCreationOptions = adoptNS([allocASCPublicKeyCredentialCreationOptionsInstance() init]);

    auto clientDataJson = WebCore::buildClientDataJson(ClientDataType::Create, options.challenge, callerOrigin.securityOrigin(), WebAuthn::Scope::SameOrigin);
    RetainPtr nsClientDataJSON = toNSData(clientDataJson->span());
    [credentialCreationOptions setClientDataJSON:nsClientDataJSON.get()];

    [credentialCreationOptions setRelyingPartyIdentifier:options.rp.id];
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
    for (PublicKeyCredentialParameters algorithmParameter : options.pubKeyCredParams)
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

static inline RetainPtr<ASCPublicKeyCredentialAssertionOptions> configureAssertionOptions(const PublicKeyCredentialRequestOptions& options, ASCPublicKeyCredentialKind kind, const std::optional<SecurityOriginData>& parentOrigin, RetainPtr<NSMutableArray<ASCPublicKeyCredentialDescriptor *>> allowedCredentials, RetainPtr<NSString> userVerification, const WebCore::SecurityOriginData& callerOrigin)
{
    // AS API makes no difference between SameSite vs CrossOrigin
    auto scope = parentOrigin ? WebAuthn::Scope::CrossOrigin : WebAuthn::Scope::SameOrigin;
    auto topOrigin = parentOrigin ? parentOrigin->toString() : nullString();
    auto clientDataJson = WebCore::buildClientDataJson(ClientDataType::Get, options.challenge, callerOrigin.securityOrigin(), scope, topOrigin);
    RetainPtr nsClientDataJSON = toNSData(clientDataJson->span());
    auto assertionOptions = adoptNS([allocASCPublicKeyCredentialAssertionOptionsInstance() initWithKind:kind relyingPartyIdentifier:options.rpId clientDataJSON:nsClientDataJSON.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get() origin:callerOrigin.toString()]);
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

static RetainPtr<ASCCredentialRequestContext> configurationAssertionRequestContext(const PublicKeyCredentialRequestOptions& options, std::optional<WebCore::MediationRequirement> mediation, std::optional<WebCore::GlobalFrameIdentifier> globalFrameID, std::optional<WebCore::SecurityOriginData>& parentOrigin, const WebCore::SecurityOriginData& callerOrigin)
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
        RetainPtr assertionOptions = configureAssertionOptions(options, ASCPublicKeyCredentialKindPlatform, parentOrigin, allowedCredentials, userVerification, callerOrigin);
        if (!assertionOptions)
            return nil;
        [requestContext setPlatformKeyCredentialAssertionOptions:assertionOptions.get()];
    }

    if (requestTypes & ASCCredentialRequestTypeSecurityKeyPublicKeyAssertion) {
        RetainPtr assertionOptions = configureAssertionOptions(options, ASCPublicKeyCredentialKindSecurityKey, parentOrigin, allowedCredentials, userVerification, callerOrigin);
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
    return AuthenticationExtensionsClientOutputs::fromCBOR(makeVector(extensionOutputsCBOR));
}
#endif // HAVE(ASC_AUTH_UI)

bool WebAuthenticatorCoordinatorProxy::isASCAvailable()
{
    return isAuthenticationServicesCoreFrameworkAvailable();
}

#if HAVE(UNIFIED_ASC_AUTH_UI)
RetainPtr<ASCCredentialRequestContext> WebAuthenticatorCoordinatorProxy::contextForRequest(WebAuthenticationRequestData&& requestData)
{
    RetainPtr<ASCCredentialRequestContext> result;
    WTF::switchOn(requestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        result = configureRegistrationRequestContext(options, requestData.globalFrameID, requestData.mediation, requestData.frameInfo.securityOrigin);
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        result = configurationAssertionRequestContext(options, requestData.mediation, requestData.globalFrameID, requestData.parentOrigin, requestData.frameInfo.securityOrigin);
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
        response.clientDataJSON = toArrayBuffer(registrationCredential.rawClientDataJSON);
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
        response.clientDataJSON = toArrayBuffer(registrationCredential.rawClientDataJSON);
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
        response.userHandle = toArrayBufferNilIfEmpty(assertionCredential.userHandle);
        response.clientDataJSON = toArrayBuffer(assertionCredential.rawClientDataJSON);
        rawAttachment = assertionCredential.attachment;
        if ([assertionCredential respondsToSelector:@selector(extensionOutputsCBOR)])
            response.extensionOutputs = toExtensionOutputs(assertionCredential.extensionOutputsCBOR);
    } else if ([credential isKindOfClass:getASCSecurityKeyPublicKeyCredentialAssertionClass()]) {
        response.isAuthenticatorAttestationResponse = false;

        ASCSecurityKeyPublicKeyCredentialAssertion *assertionCredential = credential.get();
        response.rawId = toArrayBuffer(assertionCredential.credentialID);
        response.authenticatorData = toArrayBuffer(assertionCredential.authenticatorData);
        response.signature = toArrayBuffer(assertionCredential.signature);
        response.userHandle = toArrayBufferNilIfEmpty(assertionCredential.userHandle);
        response.clientDataJSON = toArrayBuffer(assertionCredential.rawClientDataJSON);
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
    RefPtr webPageProxy = m_webPageProxy.get();
#if PLATFORM(IOS) || PLATFORM(VISION)
    if (!webPageProxy) {
        RELEASE_LOG_ERROR(WebAuthn, "WebPageProxy had been released");
        handler({ }, (AuthenticatorAttachment)0, ExceptionData { ExceptionCode::NotAllowedError, "Operation failed."_s });
    }

    if (auto* pageClient = webPageProxy->pageClient())
        requestContext.get().windowSceneIdentifier = pageClient->sceneID();

    [m_proxy performAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler)](id<ASCCredentialProtocol> credential, NSError *error) mutable {
        callOnMainRunLoop([weakThis, handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
#elif PLATFORM(MAC)
    RetainPtr<NSWindow> window = webPageProxy->platformWindow();
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
#endif // HAVE(UNIFIED_ASC_AUTH_UI)

static inline void getCanCurrentProcessAccessPasskeyForRelyingParty(const WebCore::SecurityOriginData& data, CompletionHandler<void(bool)>&& handler)
{
    if ([getASCWebKitSPISupportClass() respondsToSelector:@selector(getCanCurrentProcessAccessPasskeysForRelyingParty:withCompletionHandler:)]) {
        [getASCWebKitSPISupportClass() getCanCurrentProcessAccessPasskeysForRelyingParty:data.securityOrigin()->domain() withCompletionHandler:makeBlockPtr([handler = WTFMove(handler)](BOOL result) mutable {
            ensureOnMainRunLoop([handler = WTFMove(handler), result]() mutable {
                handler(result);
            });
        }).get()];
        return;
    }
    handler(false);
}

static inline void getArePasskeysDisallowedForRelyingParty(const WebCore::SecurityOriginData& data, CompletionHandler<void(bool)>&& handler)
{
    if ([getASCWebKitSPISupportClass() respondsToSelector:@selector(getArePasskeysDisallowedForRelyingParty:withCompletionHandler:)]) {
        [getASCWebKitSPISupportClass() getArePasskeysDisallowedForRelyingParty:data.securityOrigin()->domain() withCompletionHandler:makeBlockPtr([handler = WTFMove(handler)](BOOL result) mutable {
            ensureOnMainRunLoop([handler = WTFMove(handler), result]() mutable {
                handler(result);
            });
        }).get()];
        return;
    }
    handler(false);
}

void WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(const WebCore::SecurityOriginData& data, QueryCompletionHandler&& handler)
{
    getCanCurrentProcessAccessPasskeyForRelyingParty(data, [handler = WTFMove(handler)](bool canAccessPasskeyData) mutable {
        handler(canAccessPasskeyData && [getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]);
    });
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
        std::sort(capabilities.begin(), capabilities.end(), [] (auto& a, auto& b) {
            return codePointCompareLessThan(a.key, b.key);
        });

        ensureOnMainRunLoop([handler = WTFMove(handler), capabilities = WTFMove(capabilities)] () mutable {
            handler(WTFMove(capabilities));
        });
    }).get()];
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOriginData& data, QueryCompletionHandler&& handler)
{
    getCanCurrentProcessAccessPasskeyForRelyingParty(data, [handler = WTFMove(handler), data](bool canAccessPasskeyData) mutable {
        if (!canAccessPasskeyData) {
            handler(false);
            return;
        }
        if ([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]) {
            getArePasskeysDisallowedForRelyingParty(data, [handler = WTFMove(handler)](bool passkeysDisallowed) mutable {
                handler(!passkeysDisallowed);
            });
            return;
        }
        handler(LocalService::isAvailable());
    });
}

void WebAuthenticatorCoordinatorProxy::cancel(CompletionHandler<void()>&& handler)
{
#if HAVE(WEB_AUTHN_AS_MODERN)
    if (m_completionHandler || m_delegate) {
#else
    if (m_proxy) {
#endif
        m_cancelHandler = [weakThis = WeakPtr { *this }, handler = WTFMove(handler)]() mutable {
#if HAVE(WEB_AUTHN_AS_MODERN)
            if (weakThis) {
                weakThis->m_controller.clear();
                weakThis->m_delegate.clear();
                weakThis->m_completionHandler = nullptr;
            }
#endif
            handler();
        };
    } else
        handler();

#if HAVE(UNIFIED_ASC_AUTH_UI)
    if (m_proxy) {
        [m_proxy cancelCurrentRequest];
        m_proxy.clear();
    }
#endif

#if HAVE(WEB_AUTHN_AS_MODERN)
    if (m_controller)
        [m_controller cancel];
#endif
}

} // namespace WebKit

#endif // HAVE(UNIFIED_ASC_AUTH_UI) || HAVE(WEB_AUTHN_AS_MODERN)
