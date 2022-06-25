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

#import "AuthenticationServicesCoreSPI.h"
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(WebKit, AuthenticationServicesCore);

SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCWebKitSPISupport);

#if HAVE(ASC_AUTH_UI) || HAVE(UNIFIED_ASC_AUTH_UI)

SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCAgentProxy);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCAppleIDCredential);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCAuthorizationPresentationContext);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCAuthorizationPresenter);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCAuthorizationRemotePresenter);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCCredentialRequestContext);
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, AuthenticationServicesCore, ASCWebAuthenticationExtensionsClientInputs);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPlatformPublicKeyCredentialAssertion);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPlatformPublicKeyCredentialLoginChoice);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPlatformPublicKeyCredentialRegistration);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPublicKeyCredentialAssertionOptions);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPublicKeyCredentialCreationOptions);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPublicKeyCredentialDescriptor);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCSecurityKeyPublicKeyCredentialAssertion);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCSecurityKeyPublicKeyCredentialLoginChoice);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCSecurityKeyPublicKeyCredentialRegistration);
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, AuthenticationServicesCore, ASGlobalFrameIdentifier);

SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCAuthorizationErrorDomain, NSErrorDomain);
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, AuthenticationServicesCore, ASCPINValidationResultKey, NSString*);

#endif // HAVE(ASC_AUTH_UI)
