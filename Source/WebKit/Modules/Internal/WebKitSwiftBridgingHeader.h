/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Bridging header used by SWIFT_OBJC_BRIDGING_HEADER to expose WebKit's
// internal Objective-C[++] and C++ types to the framework's own Swift sources.
// Combined with SWIFT_BRIDGING_HEADER_IS_INTERNAL=YES, these declarations are
// only visible inside the WebKit framework target.
//
// This file is the "main file" of the bridging-header precompilation, so no
// `#pragma once` / include guard is needed (and adding one seems to trigger
// -Wpragma-once-outside-header).

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

// Suppressed across all the #imports below:
//   -Wnullability-completeness   — once any #imported header is annotated,
//     Clang demands every raw pointer in the whole TU also be annotated;
//     that "virality" is a known limitation in WebKit.
//   -Wpragma-clang-attribute and -Wignored-attributes — the audit-using
//     headers in the first block below open
//     NS_HEADER_AUDIT_BEGIN(..., sendability), whose attribute push
//     complains about record/enum declarations already in scope from
//     headers each SPI header itself transitively #imports. The push
//     still applies correctly to the contents of the audit region.
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
IGNORE_CLANG_WARNINGS_BEGIN("pragma-clang-attribute")
IGNORE_CLANG_WARNINGS_BEGIN("ignored-attributes")

#if PLATFORM(COCOA)
// Headers that open an NS_HEADER_AUDIT_BEGIN(..., sendability) region are
// imported first: each one's audit pragma applies to records/enums (via
// `apply_to = any(..., enum)`), which Clang rejects if a matching
// declaration is already in scope. Putting them ahead of the C++ headers
// below keeps each audit pragma the first thing Clang sees in this TU
// matching its apply_to clause.
#import "AppKitSPI.h"
#import "ModelTypes.h"
#import "UIWindowScene+Extras.h"
#import "WKDeferringGestureRecognizer.h"
#import "WKMaterialHostingSupport.h"
#import "WKMouseDeviceObserver.h"
#import "WKPDFHUDView.h"
#import "WKSeparatedImageView.h"
#import "WKStageModeOrbitSimulator.h"
#import "WKSurroundingsEffect.h"
#import "WKTextSelectionController.h"
#import "WKTextSelectionRect.h"
#import "WKUSDStageConverter.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "_WKTextExtractionInternal.h"

// Remaining Obj-C[++] internal headers that don't open an audit region.
#import "WKObject.h"
#import "WKPreferencesInternal.h"
#import "WKScrollGeometry.h"
#import "WKTextEffectManager.h"
#import "WKUIDelegateInternal.h"
#import "WKWebViewIOS.h"
#import "WebViewImpl.h"
#endif // PLATFORM(COCOA)

// C++ headers.
#import "APIArray.h"
#import "APICustomProtocolManagerClient.h"
#import "APIHistoryClient.h"
#import "APINavigationClient.h"
#import "APIObject.h"
#import "AuxiliaryProcessProxy.h"
#import "CallbackID.h"
#import "FrameInfoData.h"
#import "FrameTreeNodeData.h"
#import "GamepadData.h"
#if PLATFORM(COCOA)
#import "GestureTypes.h"
#endif
#import "IPCTesterReceiverMessages.h"
#import "JSHandleInfo.h"
#import "JavaScriptEvaluationResult.h"
#import "LoadedWebArchive.h"
#import "MessageReceiver.h"
#import "SessionState.h"
#import "SuspendedPageProxy.h"
#import "SwiftDemoLogoConfirmation.h"
#if PLATFORM(IOS_FAMILY)
#import "WebAutocorrectionData.h"
#endif
#import "WebBackForwardCache.h"
#import "WebBackForwardCacheEntry.h"
#import "WebBackForwardListCounts.h"
#import "WebBackForwardListFrameItem.h"
#import "WebBackForwardListItem.h"
#import "WebBackForwardListMessages.h"
#import "WebBackForwardListSwiftUtilities.h"
#import "WebExtensionCookieParameters.h"
#import "WebFrameMetrics.h"
#import "WebFrameProxy.h"
#import "WebInspectorUtilities.h"
#import "WebKeyboardEvent.h"
#import "WebNavigationState.h"
#import "WebPageCreationParameters.h"
#import "WebPageInspectorController.h"
#import "WebPageProxy.h"
#import "WebPageProxyInternals.h"
#import "WebPermissionControllerProxy.h"
#import "WebProcessActivityState.h"
#import "WebProcessProxy.h"
#import "WebPushMessage.h"
#import "WebsiteData.h"

IGNORE_CLANG_WARNINGS_END
IGNORE_CLANG_WARNINGS_END
IGNORE_CLANG_WARNINGS_END
