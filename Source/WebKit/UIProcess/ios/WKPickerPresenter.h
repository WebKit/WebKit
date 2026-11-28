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

#pragma once

#if PLATFORM(IOS_FAMILY)

#import <wtf/Forward.h>

@class WKContentView;
@class WKFileUploadPanel;

namespace API {
class OpenPanelParameters;
}

namespace WebCore {
class FloatRect;
class IntRect;
struct ContactInfo;
struct ContactsRequestData;
struct ShareDataWithParsedURL;
enum class RouteSharingPolicy : uint8_t;
#if ENABLE(WEB_AUTHN)
struct DigitalCredentialsRequestData;
struct DigitalCredentialsResponseData;
struct ExceptionData;
#endif
}

namespace WebKit {
class WebOpenPanelResultListenerProxy;
struct FrameInfoData;
enum class PickerDismissalReason : uint8_t;
}

@interface WKPickerPresenter : NSObject

- (instancetype)initWithContentView:(WKContentView *)contentView NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect routeSharingPolicy:(WebCore::RouteSharingPolicy)routeSharingPolicy routingContextUID:(NSString *)routingContextUID;
- (BOOL)showFileUploadPanelWithParameters:(API::OpenPanelParameters*)parameters frameInfo:(const WebKit::FrameInfoData&)frameInfo resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener;
- (void)showShareSheet:(const WebCore::ShareDataWithParsedURL&)data inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(CompletionHandler<void(bool)>&&)completionHandler;
- (void)showContactPicker:(const WebCore::ContactsRequestData&)requestData completionHandler:(CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&)completionHandler;
#if ENABLE(WEB_AUTHN)
- (void)showDigitalCredentialsChooser:(const WebCore::DigitalCredentialsRequestData&)requestData completionHandler:(CompletionHandler<void(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&)completionHandler;
- (void)dismissDigitalCredentialsChooser:(CompletionHandler<void(bool)>&&)completionHandler;
#endif

- (void)dismissPickersIfNeededWithReason:(WebKit::PickerDismissalReason)reason;

@property (nonatomic, readonly) WKFileUploadPanel *fileUploadPanel;

- (NSArray<NSString *> *)filePickerAcceptedTypeIdentifiers;
- (void)dismissFilePicker;
- (void)dismissContactPickerWithContacts:(NSArray *)contacts;

@end

#endif // PLATFORM(IOS_FAMILY)
