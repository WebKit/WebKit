/**
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

// This file is a no-op unless we have a feature where C++ calls into Swift
// or uses Swift data types.
#if ENABLE(SWIFT_DEMO_URI_SCHEME) || ENABLE(IPC_TESTING_SWIFT) || ENABLE(BACK_FORWARD_LIST_SWIFT)

#ifdef __swift__
#warning "You're including WebKit-Swift.h from a C++ header file - don't do that. This may cause circular Swift<->C++ dependencies and build problems."
#endif

// Anything needing to use Swift types or functions should include
// this rather than directly including WebKit-Swift-Generated.h. Its purposes:
// - include any pre-requisite headers
// - set up warnings suitably
// - select between the headers generated using built-in or custom build
//   actions on different SDK versions

#include <wtf/Platform.h>

// If Swift function parameters or return types depend on C++ types, the
// relevant headers must be included here. rdar://165068038
#include "APIArray.h"
#include "IPCTesterReceiverMessages.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListMessages.h"
#include "WebBackForwardListSwiftUtilities.h"
#include "WebPageProxy.h"

#ifdef __OBJC__
#include "WKSeparatedImageView.h"
#include "WKUIDelegatePrivate.h"
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebKit-Swift-Additions.h>
#endif

// rdar://165192318
IGNORE_CLANG_WARNINGS_BEGIN("arc-bridge-casts-disallowed-in-nonarc")
// rdar://171345626
IGNORE_CLANG_WARNINGS_BEGIN("objc-property-no-attribute")
#ifdef GENERATE_SINGLE_SWIFT_INTEROP_FILE
#include "WebKit-Swift-Generated.h"
#else
#include "WebKit-Swift-CPP.h"
#endif
IGNORE_CLANG_WARNINGS_END
IGNORE_CLANG_WARNINGS_END

#endif // ENABLE(SWIFT_DEMO_URI_SCHEME) || ENABLE(IPC_TESTING_SWIFT) || ENABLE(BACK_FORWARD_LIST_SWIFT)
