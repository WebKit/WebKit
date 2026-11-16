/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"

#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebKit, AuthenticationServices);

SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationController);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPlatformPublicKeyCredentialProvider);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationSecurityKeyPublicKeyCredentialProvider);
#if HAVE(WEB_AUTHN_PUBLIC_KEY_CREDENTIAL_MANAGER)
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationWebBrowserPublicKeyCredentialManager);
#endif
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASPublicKeyCredentialClientData);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPlatformPublicKeyCredentialRegistration);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationSecurityKeyPublicKeyCredentialRegistration);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPlatformPublicKeyCredentialAssertion);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPublicKeyCredentialParameters);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPlatformPublicKeyCredentialDescriptor);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationSecurityKeyPublicKeyCredentialAssertion);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationErrorDomain, NSErrorDomain);
#if HAVE(WEB_AUTHN_PRF_API)
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPublicKeyCredentialPRFRegistrationInput);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServices, ASAuthorizationPublicKeyCredentialPRFAssertionInput);
#endif
