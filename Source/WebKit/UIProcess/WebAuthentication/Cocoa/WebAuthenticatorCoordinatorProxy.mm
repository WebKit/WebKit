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
#import "WKError.h"
#import "WebAuthenticationRequestData.h"
#import "WebPageProxy.h"
#import <WebCore/AuthenticatorAttachment.h>
#import <WebCore/AuthenticatorResponseData.h>
#import <WebCore/BufferSource.h>
#import <WebCore/ExceptionData.h>
#import <WebCore/PublicKeyCredentialCreationOptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

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
    if ([allocASCWebAuthenticationExtensionsClientInputsInstance() respondsToSelector:@selector(initWithAppID:isGoogleLegacyAppIDSupport:)])
        return adoptNS([allocASCWebAuthenticationExtensionsClientInputsInstance() initWithAppID:extensions.appid isGoogleLegacyAppIDSupport:extensions.googleLegacyAppidSupport]);

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

static RetainPtr<ASCCredentialRequestContext> configureRegistrationRequestContext(const PublicKeyCredentialCreationOptions& options, const Vector<uint8_t>& hash, std::optional<WebCore::GlobalFrameIdentifier> globalFrameID)
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
        assertionOptions.get().destinationSiteForCrossSiteAssertion = parentOrigin->toString();
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
            transports.uncheckedAppend(static_cast<WebCore::AuthenticatorTransport>(ascTransport.intValue));
    }
    return transports;
}

static std::optional<AuthenticationExtensionsClientOutputs> toExtensionOutputs(NSData *extensionOutputsCBOR)
{
    if (!extensionOutputsCBOR)
        return std::nullopt;
    return AuthenticationExtensionsClientOutputs::fromCBOR(vectorFromNSData(extensionOutputsCBOR));
}

RetainPtr<ASCCredentialRequestContext> WebAuthenticatorCoordinatorProxy::contextForRequest(WebAuthenticationRequestData&& requestData)
{
    RetainPtr<ASCCredentialRequestContext> result;
    WTF::switchOn(requestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        result = configureRegistrationRequestContext(options, requestData.hash, requestData.globalFrameID);
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
            exceptionCode = NotAllowedError;

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
    else {
        ASSERT([rawAttachment isEqualToString:@"cross-platform"]);
        attachment = AuthenticatorAttachment::CrossPlatform;
    }

    handler(response, attachment, exceptionData);
}

void WebAuthenticatorCoordinatorProxy::performRequest(RetainPtr<ASCCredentialRequestContext> requestContext, RequestCompletionHandler&& handler)
{
    if (requestContext.get().requestTypes == ASCCredentialRequestTypeNone) {
        handler({ }, (AuthenticatorAttachment)0, ExceptionData { NotAllowedError, "This request has been cancelled by the user."_s });
        RELEASE_LOG_ERROR(WebAuthn, "Request cancelled due to none requestTypes.");
        return;
    }
    m_proxy = adoptNS([allocASCAgentProxyInstance() init]);
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if ([requestContext respondsToSelector:@selector(requestStyle)] && requestContext.get().requestStyle == ASCredentialRequestStyleAutoFill) {
        [m_proxy performAutoFillAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler)](id<ASCCredentialProtocol> credential, NSError *error) mutable {
            ensureOnMainRunLoop([weakThis, handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
                if (!weakThis) {
                    handler({ }, (AuthenticatorAttachment)0, ExceptionData { NotAllowedError, "Operation failed."_s });
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
#if PLATFORM(IOS)
    [m_proxy performAuthorizationRequestsForContext:requestContext.get() withCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler)](id<ASCCredentialProtocol> credential, NSError *error) mutable {
        callOnMainRunLoop([weakThis, handler = WTFMove(handler), credential = retainPtr(credential), error = retainPtr(error)] () mutable {
#elif PLATFORM(MAC)
    RetainPtr<NSWindow> window = m_webPageProxy.platformWindow();
    [m_proxy performAuthorizationRequestsForContext:requestContext.get() withClearanceHandler:makeBlockPtr([weakThis = WeakPtr { *this }, handler = WTFMove(handler), window = WTFMove(window)](NSXPCListenerEndpoint *daemonEndpoint, NSError *error) mutable {
        callOnMainRunLoop([weakThis, handler = WTFMove(handler), window = WTFMove(window), daemonEndpoint = retainPtr(daemonEndpoint), error = retainPtr(error)] () mutable {
            if (!weakThis || !daemonEndpoint) {
                RELEASE_LOG_ERROR(WebAuthn, "Could not connect to authorization daemon: %@.", error.get().localizedDescription);
                handler({ }, (AuthenticatorAttachment)0, ExceptionData { NotAllowedError, "Operation failed."_s });
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

void WebAuthenticatorCoordinatorProxy::isConditionalMediationAvailable(QueryCompletionHandler&& handler)
{
    handler([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]);
}

void WebAuthenticatorCoordinatorProxy::isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOriginData& data, QueryCompletionHandler&& handler)
{
    if ([getASCWebKitSPISupportClass() shouldUseAlternateCredentialStore]) {
        handler(![getASCWebKitSPISupportClass() respondsToSelector:@selector(arePasskeysDisallowedForRelyingParty:)] || ![getASCWebKitSPISupportClass() arePasskeysDisallowedForRelyingParty:data.securityOrigin()->domain()]);
        return;
    }

    handler(LocalService::isAvailable());
}

void WebAuthenticatorCoordinatorProxy::cancel()
{
    if (m_proxy) {
        [m_proxy cancelCurrentRequest];
        m_proxy.clear();
    }
}

} // namespace WebKit

#endif // HAVE(UNIFIED_ASC_AUTH_UI)
