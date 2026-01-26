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

#pragma once

#import "AuthenticationServicesForwardDeclarations.h"
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(WebKit, AuthenticationServices);

SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorization);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationController);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPlatformPublicKeyCredentialProvider);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationSecurityKeyPublicKeyCredentialProvider);
#if HAVE(WEB_AUTHN_PUBLIC_KEY_CREDENTIAL_MANAGER)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationWebBrowserPublicKeyCredentialManager);
#endif
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationWebBrowserPlatformPublicKeyCredentialProvider);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASPublicKeyCredentialClientData);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPlatformPublicKeyCredentialRegistration);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationSecurityKeyPublicKeyCredentialRegistration);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPlatformPublicKeyCredentialAssertion);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationControllerDelegate);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPublicKeyCredentialParameters);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPlatformPublicKeyCredentialDescriptor);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationSecurityKeyPublicKeyCredentialAssertion);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput);
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(WebKit, AuthenticationServices, ASAuthorizationErrorDomain, NSErrorDomain);
#if HAVE(WEB_AUTHN_PRF_API)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPublicKeyCredentialPRFRegistrationInput);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues);
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(WebKit, ASAuthorizationPublicKeyCredentialPRFAssertionInput);
#endif
#define ASAuthorizationErrorDomain WebKit::get_AuthenticationServices_ASAuthorizationErrorDomainSingleton()
