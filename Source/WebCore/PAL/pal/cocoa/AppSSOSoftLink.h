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

#pragma once

#if HAVE(APP_SSO)

#import <pal/spi/cocoa/AppSSOSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AppSSO);

SOFT_LINK_CLASS_FOR_HEADER(PAL, SOAuthorization);

SOFT_LINK_CLASS_FOR_HEADER(PAL, SOAuthorizationHints);

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, AppSSO, SOAuthorizationOptionUserActionInitiated, NSString*);
#define SOAuthorizationOptionUserActionInitiated PAL::get_AppSSO_SOAuthorizationOptionUserActionInitiated()

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, AppSSO, SOAuthorizationOptionInitiatorOrigin, NSString*);
#define SOAuthorizationOptionInitiatorOrigin PAL::get_AppSSO_SOAuthorizationOptionInitiatorOrigin()

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, AppSSO, SOAuthorizationOptionInitiatingAction, NSString*);
#define SOAuthorizationOptionInitiatingAction PAL::get_AppSSO_SOAuthorizationOptionInitiatingAction()

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, AppSSO, SOErrorDomain, NSErrorDomain);
#define SOErrorDomain PAL::get_AppSSO_SOErrorDomain()

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AppSSOCore);

SOFT_LINK_CLASS_FOR_HEADER(PAL, SOAuthorizationHintsCore);

#endif
