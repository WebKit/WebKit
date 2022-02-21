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
#import "WKError.h"
#import "WebAuthenticationRequestData.h"
#import "WebPageProxy.h"
#import <AuthenticationServices/ASCOSEConstants.h>
#import <WebCore/AuthenticatorAttachment.h>
#import <WebCore/AuthenticatorResponseData.h>
#import <WebCore/BufferSource.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <wtf/BlockPtr.h>

#import "AuthenticationServicesCoreSoftLink.h"

namespace WebKit {
using namespace WebCore;

static inline Ref<ArrayBuffer> toArrayBuffer(NSData *data)
{
    return ArrayBuffer::create(reinterpret_cast<const uint8_t*>(data.bytes), data.length);
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
    }

    return @"none";
}

static inline ExceptionCode toExceptionCode(NSInteger nsErrorCode)
{
    ExceptionCode exceptionCode = (ExceptionCode)nsErrorCode;

    switch (exceptionCode) {
    case IndexSizeError: FALLTHROUGH;
    case HierarchyRequestError: FALLTHROUGH;
    case WrongDocumentError: FALLTHROUGH;
    case InvalidCharacterError: FALLTHROUGH;
    case NoModificationAllowedError: FALLTHROUGH;
    case NotFoundError: FALLTHROUGH;
    case NotSupportedError: FALLTHROUGH;
    case InUseAttributeError: FALLTHROUGH;
    case InvalidStateError: FALLTHROUGH;
    case SyntaxError: FALLTHROUGH;
    case InvalidModificationError: FALLTHROUGH;
    case NamespaceError: FALLTHROUGH;
    case InvalidAccessError: FALLTHROUGH;
    case TypeMismatchError: FALLTHROUGH;
    case SecurityError: FALLTHROUGH;
    case NetworkError: FALLTHROUGH;
    case AbortError: FALLTHROUGH;
    case URLMismatchError: FALLTHROUGH;
    case QuotaExceededError: FALLTHROUGH;
    case TimeoutError: FALLTHROUGH;
    case InvalidNodeTypeError: FALLTHROUGH;
    case DataCloneError: FALLTHROUGH;
    case EncodingError: FALLTHROUGH;
    case NotReadableError: FALLTHROUGH;
    case UnknownError: FALLTHROUGH;
    case ConstraintError: FALLTHROUGH;
    case DataError: FALLTHROUGH;
    case TransactionInactiveError: FALLTHROUGH;
    case ReadonlyError: FALLTHROUGH;
    case VersionError: FALLTHROUGH;
    case OperationError: FALLTHROUGH;
    case NotAllowedError: FALLTHROUGH;
    case RangeError: FALLTHROUGH;
    case TypeError: FALLTHROUGH;
    case JSSyntaxError: FALLTHROUGH;
    case StackOverflowError: FALLTHROUGH;
    case OutOfMemoryError: FALLTHROUGH;
    case ExistingExceptionError:
        return exceptionCode;
    }

    return NotAllowedError;
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
            }

            if (transportString)
                [transports addObject:transportString];
        }
    }

    return adoptNS([allocASCPublicKeyCredentialDescriptorInstance() initWithCredentialID:WebCore::toNSData(descriptor.id).get() transports:transports.get()]);
}

static inline RetainPtr<ASCWebAuthenticationExtensionsClientInputs> toASCExtensions(const AuthenticationExtensionsClientInputs& extensions)
{
    if ([allocASCWebAuthenticationExtensionsClientInputsInstance() respondsToSelector:@selector(initWithAppID:isGoogleLegacyAppIDSupport:)])
        return adoptNS([allocASCWebAuthenticationExtensionsClientInputsInstance() initWithAppID:extensions.appid isGoogleLegacyAppIDSupport:extensions.googleLegacyAppidSupport]);

    return nil;
}

static RetainPtr<ASCCredentialRequestContext> configureRegistrationRequestContext(const PublicKeyCredentialCreationOptions& options, Vector<uint8_t> hash)
{
    ASCCredentialRequestTypes requestTypes = ASCCredentialRequestTypePlatformPublicKeyRegistration | ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration;

    RetainPtr<NSString> userVerification;
    bool shouldRequireResidentKey = false;
    std::optional<PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria> authenticatorSelection = options.authenticatorSelection;
    if (authenticatorSelection) {
        std::optional<AuthenticatorAttachment> attachment = authenticatorSelection->authenticatorAttachment;
        if (attachment == AuthenticatorAttachment::Platform)
            requestTypes = ASCCredentialRequestTypePlatformPublicKeyRegistration;
        else if (attachment == AuthenticatorAttachment::CrossPlatform)
            requestTypes = ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration;

        userVerification = toNSString(authenticatorSelection->userVerification);

        shouldRequireResidentKey = authenticatorSelection->requireResidentKey;
    }

    auto requestContext = adoptNS([allocASCCredentialRequestContextInstance() initWithRequestTypes:requestTypes]);
    [requestContext setRelyingPartyIdentifier:options.rp.id];

    auto credentialCreationOptions = adoptNS([allocASCPublicKeyCredentialCreationOptionsInstance() init]);

    if ([credentialCreationOptions respondsToSelector:@selector(setClientDataHash:)])
        [credentialCreationOptions setClientDataHash:toNSData(hash).get()];
    else
        [credentialCreationOptions setChallenge:WebCore::toNSData(options.challenge).get()];
    [credentialCreationOptions setRelyingPartyIdentifier:options.rp.id];
    [credentialCreationOptions setUserName:options.user.name];
    [credentialCreationOptions setUserIdentifier:WebCore::toNSData(options.user.id).get()];
    [credentialCreationOptions setUserDisplayName:options.user.displayName];
    [credentialCreationOptions setUserVerificationPreference:userVerification.get()];
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

    if (requestTypes & ASCCredentialRequestTypePlatformPublicKeyRegistration)
        [requestContext setPlatformKeyCredentialCreationOptions:credentialCreationOptions.get()];

    if (requestTypes & ASCCredentialRequestTypeSecurityKeyPublicKeyRegistration)
        [requestContext setSecurityKeyCredentialCreationOptions:credentialCreationOptions.get()];

    if (options.extensions && [credentialCreationOptions respondsToSelector:@selector(setExtensions:)])
        [credentialCreationOptions setExtensions:toASCExtensions(*options.extensions).get()];

    return requestContext;
}

static RetainPtr<ASCCredentialRequestContext> configurationAssertionRequestContext(const PublicKeyCredentialRequestOptions& options, Vector<uint8_t> hash, std::optional<WebCore::MediationRequirement> mediation)
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
    if (mediation == MediationRequirement::Conditional && [requestContext respondsToSelector:@selector(setRequestStyle:)])
        requestContext.get().requestStyle = ASCredentialRequestStyleAutoFill;

    if (requestTypes & ASCCredentialRequestTypePlatformPublicKeyAssertion) {
        auto assertionOptions = adoptNS(allocASCPublicKeyCredentialAssertionOptionsInstance());
        if ([assertionOptions respondsToSelector:@selector(initWithKind:relyingPartyIdentifier:clientDataHash:userVerificationPreference:allowedCredentials:)]) {
            auto nsHash = toNSData(hash);
            [assertionOptions initWithKind:ASCPublicKeyCredentialKindPlatform relyingPartyIdentifier:options.rpId clientDataHash:nsHash.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get()];
        } else {
            auto challenge = WebCore::toNSData(options.challenge);
            [assertionOptions initWithKind:ASCPublicKeyCredentialKindPlatform relyingPartyIdentifier:options.rpId challenge:challenge.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get()];
        }
        if (options.extensions && [assertionOptions respondsToSelector:@selector(setExtensions:)])
            [assertionOptions setExtensions:toASCExtensions(*options.extensions).get()];

        [requestContext setPlatformKeyCredentialAssertionOptions:assertionOptions.get()];
    }

    if (requestTypes & ASCCredentialRequestTypeSecurityKeyPublicKeyAssertion) {
        auto assertionOptions = adoptNS(allocASCPublicKeyCredentialAssertionOptionsInstance());
        if ([assertionOptions respondsToSelector:@selector(initWithKind:relyingPartyIdentifier:clientDataHash:userVerificationPreference:allowedCredentials:)]) {
            auto nsHash = toNSData(hash);
            [assertionOptions initWithKind:ASCPublicKeyCredentialKindSecurityKey relyingPartyIdentifier:options.rpId clientDataHash:nsHash.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get()];
        } else {
            auto challenge = WebCore::toNSData(options.challenge);
            [assertionOptions initWithKind:ASCPublicKeyCredentialKindSecurityKey relyingPartyIdentifier:options.rpId challenge:challenge.get() userVerificationPreference:userVerification.get() allowedCredentials:allowedCredentials.get()];
        }
        if (options.extensions && [assertionOptions respondsToSelector:@selector(setExtensions:)])
            [assertionOptions setExtensions:toASCExtensions(*options.extensions).get()];

        [requestContext setSecurityKeyCredentialAssertionOptions:assertionOptions.get()];
    }

    return requestContext;
}

RetainPtr<ASCCredentialRequestContext> WebAuthenticatorCoordinatorProxy::contextForRequest(WebAuthenticationRequestData&& requestData)
{
    RetainPtr<ASCCredentialRequestContext> result;
    WTF::switchOn(requestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        result = configureRegistrationRequestContext(options, requestData.hash);
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        result = configurationAssertionRequestContext(options, requestData.hash, requestData.mediation);
    });
    return result;
}

void WebAuthenticatorCoordinatorProxy::performRequest(RetainPtr<ASCCredentialRequestContext> requestContext, RequestCompletionHandler&& handler)
{
    auto proxy = adoptNS([allocASCAgentProxyInstance() init]);

    auto completionHandler = makeBlockPtr([handler = WTFMove(handler)](id <ASCCredentialProtocol> credential, NSError *error) mutable {
        ensureOnMainRunLoop([handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
            AuthenticatorResponseData response;
            AuthenticatorAttachment attachment;
            ExceptionData exceptionData;

            if ([credential isKindOfClass:getASCPlatformPublicKeyCredentialRegistrationClass()]) {
                attachment = AuthenticatorAttachment::Platform;
                response.isAuthenticatorAttestationResponse = true;

                ASCPlatformPublicKeyCredentialRegistration *registrationCredential = credential.get();
                response.rawId = toArrayBuffer(registrationCredential.credentialID);
                response.attestationObject = toArrayBuffer(registrationCredential.attestationObject);
            } else if ([credential isKindOfClass:getASCSecurityKeyPublicKeyCredentialRegistrationClass()]) {
                attachment = AuthenticatorAttachment::CrossPlatform;
                response.isAuthenticatorAttestationResponse = true;

                ASCSecurityKeyPublicKeyCredentialRegistration *registrationCredential = credential.get();
                response.rawId = toArrayBuffer(registrationCredential.credentialID);
                response.attestationObject = toArrayBuffer(registrationCredential.attestationObject);
            } else if ([credential isKindOfClass:getASCPlatformPublicKeyCredentialAssertionClass()]) {
                attachment = AuthenticatorAttachment::Platform;
                response.isAuthenticatorAttestationResponse = false;

                ASCPlatformPublicKeyCredentialAssertion *assertionCredential = credential.get();
                response.rawId = toArrayBuffer(assertionCredential.credentialID);
                response.authenticatorData = toArrayBuffer(assertionCredential.authenticatorData);
                response.signature = toArrayBuffer(assertionCredential.signature);
                response.userHandle = toArrayBuffer(assertionCredential.userHandle);
            } else if ([credential isKindOfClass:getASCSecurityKeyPublicKeyCredentialAssertionClass()]) {
                attachment = AuthenticatorAttachment::CrossPlatform;
                response.isAuthenticatorAttestationResponse = false;

                ASCSecurityKeyPublicKeyCredentialAssertion *assertionCredential = credential.get();
                response.rawId = toArrayBuffer(assertionCredential.credentialID);
                response.authenticatorData = toArrayBuffer(assertionCredential.authenticatorData);
                response.signature = toArrayBuffer(assertionCredential.signature);
                response.userHandle = toArrayBuffer(assertionCredential.userHandle);
            } else {
                attachment = (AuthenticatorAttachment) 0;
                ExceptionCode exceptionCode;
                NSString *errorMessage = nil;
                if ([error.get().domain isEqualToString:WKErrorDomain]) {
                    exceptionCode = toExceptionCode(error.get().code);
                    errorMessage = error.get().userInfo[NSLocalizedDescriptionKey];
                } else {
                    exceptionCode = NotAllowedError;

                    if ([error.get().domain isEqualToString:ASCAuthorizationErrorDomain] && error.get().code == ASCAuthorizationErrorUserCanceled)
                        errorMessage = @"This request has been cancelled by the user.";
                    else
                        errorMessage = @"Operation failed.";
                }

                exceptionData = { exceptionCode, errorMessage };
            }

            handler(response, attachment, exceptionData);
        });
    });
    
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if ([requestContext respondsToSelector:@selector(requestStyle)] && requestContext.get().requestStyle == ASCredentialRequestStyleAutoFill) {
        [proxy performAutoFillAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([proxy = WTFMove(proxy), completionHandler = WTFMove(completionHandler)](id <ASCCredentialProtocol> credential, NSError *error) mutable {
            completionHandler(credential, error);
        }).get()];
        return;
    }
#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)
    
#if PLATFORM(IOS)
    [proxy performAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([proxy = WTFMove(proxy), completionHandler = WTFMove(completionHandler)](id <ASCCredentialProtocol> credential, NSError *error) mutable {
        completionHandler(credential, error);
    }).get()];
#elif PLATFORM(MAC)
    RetainPtr<NSWindow> window = m_webPageProxy.platformWindow();
    [proxy performAuthorizationRequestsForContext:requestContext.get() withClearanceHandler:makeBlockPtr([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler), window = WTFMove(window), proxy = WTFMove(proxy)](NSXPCListenerEndpoint *daemonEndpoint, NSError *error) mutable {
        callOnMainRunLoop([weakThis, completionHandler = WTFMove(completionHandler), window = WTFMove(window), proxy = WTFMove(proxy), daemonEndpoint = retainPtr(daemonEndpoint), error = retainPtr(error)] () mutable {
            if (!weakThis || !daemonEndpoint) {
                LOG_ERROR("Could not connect to authorization daemon: %@\n", error.get());
                completionHandler({ }, error.get());
                return;
            }

            weakThis->m_presenter = adoptNS([allocASCAuthorizationRemotePresenterInstance() init]);
            [weakThis->m_presenter presentWithWindow:window.get() daemonEndpoint:daemonEndpoint.get() completionHandler:makeBlockPtr([proxy = WTFMove(proxy), completionHandler = WTFMove(completionHandler)](id <ASCCredentialProtocol> credential, NSError *error) mutable {
                completionHandler(credential, error);
            }).get()];
        });
    }).get()];
#endif // PLATFORM(MAC)
}

void WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(QueryCompletionHandler&& handler)
{
    handler([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]);
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&& handler)
{
    if ([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]) {
        handler(true);
        return;
    }

    handler(LocalService::isAvailable());
}

} // namespace WebKit

#endif // HAVE(UNIFIED_ASC_AUTH_UI)
