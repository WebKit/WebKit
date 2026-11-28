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

#import "config.h"
#import "WKPickerPresenter.h"

#if PLATFORM(IOS_FAMILY)

#import "APIFrameInfo.h"
#import "FrameInfoData.h"
#import "Logging.h"
#import "PickerDismissalReason.h"
#import "UIKitSPI.h"
#import "WKContentView.h"
#import "WKFrameInfoInternal.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <WebCore/ContactInfo.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <WebKit/WKAirPlayRoutePicker.h>
#import <WebKit/WKContactPicker.h>
#import <WebKit/WKFileUploadPanel.h>
#import <WebKit/WKShareSheet.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if ENABLE(WEB_AUTHN)
#import "WKDigitalCredentialsPicker.h"
#endif

@interface WKPickerPresenter () <WKFileUploadPanelDelegate
#if HAVE(SHARE_SHEET_UI)
    , WKShareSheetDelegate
#endif
#if HAVE(CONTACTSUI)
    , WKContactPickerDelegate
#endif
>
@end

@implementation WKPickerPresenter {
    WeakObjCPtr<WKContentView> _contentView;
#if ENABLE(AIRPLAY_PICKER)
    RetainPtr<WKAirPlayRoutePicker> _airPlayRoutePicker;
#endif
    RetainPtr<WKFileUploadPanel> _fileUploadPanel;
    std::optional<WebKit::FrameInfoData> _frameInfoForFileUploadPanel;
#if HAVE(SHARE_SHEET_UI)
    RetainPtr<WKShareSheet> _shareSheet;
#endif
#if HAVE(CONTACTSUI)
    RetainPtr<WKContactPicker> _contactPicker;
#endif
#if ENABLE(WEB_AUTHN)
    RetainPtr<WKDigitalCredentialsPicker> _digitalCredentialsPicker;
#endif
}

- (instancetype)initWithContentView:(WKContentView *)contentView
{
    if (!(self = [super init]))
        return nil;

    _contentView = contentView;
    return self;
}

- (WKFileUploadPanel *)fileUploadPanel
{
    return _fileUploadPanel.get();
}

- (void)showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect routeSharingPolicy:(WebCore::RouteSharingPolicy)routeSharingPolicy routingContextUID:(NSString *)routingContextUID
{
#if ENABLE(AIRPLAY_PICKER)
    if (!_airPlayRoutePicker)
        _airPlayRoutePicker = adoptNS([[WKAirPlayRoutePicker alloc] init]);
    [_airPlayRoutePicker showFromView:_contentView.get().get() routeSharingPolicy:routeSharingPolicy routingContextUID:routingContextUID hasVideo:hasVideo];
#endif
}

- (BOOL)showFileUploadPanelWithParameters:(API::OpenPanelParameters*)parameters frameInfo:(const WebKit::FrameInfoData&)frameInfo resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener
{
    // The panel still showing may belong to a frame whose process has exited.
    if (_fileUploadPanel)
        return NO;

    _frameInfoForFileUploadPanel = frameInfo;
    _fileUploadPanel = adoptNS([[WKFileUploadPanel alloc] initWithView:_contentView.get().get()]);
    [_fileUploadPanel setDelegate:self];
    [_fileUploadPanel presentWithParameters:parameters resultListener:listener];
    return YES;
}

- (void)fileUploadPanelDidDismiss:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);

    RetainPtr contentView = _contentView.get();
    if ([contentView window] && ![[contentView window] firstResponder])
        [contentView becomeFirstResponder];

    [_fileUploadPanel setDelegate:nil];
    _fileUploadPanel = nil;
}

- (BOOL)fileUploadPanelDestinationIsManaged:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);

    RetainPtr webView = [_contentView.get() webView];
    id<WKUIDelegatePrivate> uiDelegate = static_cast<id<WKUIDelegatePrivate>>([webView UIDelegate]);
    return [uiDelegate respondsToSelector:@selector(_webView:fileUploadPanelContentIsManagedWithInitiatingFrame:)]
        && [uiDelegate _webView:webView.get() fileUploadPanelContentIsManagedWithInitiatingFrame:_frameInfoForFileUploadPanel ? wrapper(API::FrameInfo::create(*std::exchange(_frameInfoForFileUploadPanel, std::nullopt))).get() : nil];
}

#if HAVE(PHOTOS_UI)
- (BOOL)fileUploadPanelPhotoPickerPrefersOriginalImageFormat:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);
    RefPtr page = [_contentView.get() page];
    if (!page)
        return NO;
    return protect(page->preferences())->photoPickerPrefersOriginalImageFormat();
}
#endif

- (void)showShareSheet:(const WebCore::ShareDataWithParsedURL&)data inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
{
#if HAVE(SHARE_SHEET_UI)
    if (_shareSheet)
        [_shareSheet dismissIfNeededWithReason:WebKit::PickerDismissalReason::ResetState];

    _shareSheet = adoptNS([[WKShareSheet alloc] initWithView:[_contentView.get() webView]]);
    [_shareSheet setDelegate:self];
    [_shareSheet presentWithParameters:data inRect:rect completionHandler:WTF::move(completionHandler)];
#endif // HAVE(SHARE_SHEET_UI)
}

#if HAVE(SHARE_SHEET_UI)

- (void)shareSheetDidDismiss:(WKShareSheet *)shareSheet
{
    ASSERT(_shareSheet == shareSheet);

    [_shareSheet setDelegate:nil];
    _shareSheet = nil;
}

- (void)shareSheet:(WKShareSheet *)shareSheet willShowActivityItems:(NSArray *)activityItems
{
    ASSERT(_shareSheet == shareSheet);

    RetainPtr webView = [_contentView.get() webView];
    RetainPtr uiDelegate = static_cast<id<WKUIDelegatePrivate>>([webView UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_webView:willShareActivityItems:)])
        [uiDelegate _webView:webView.get() willShareActivityItems:activityItems];
}

#endif // HAVE(SHARE_SHEET_UI)

#if ENABLE(WEB_AUTHN)
- (void)showDigitalCredentialsChooser:(const WebCore::DigitalCredentialsRequestData&)requestData completionHandler:(CompletionHandler<void(std::expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&)completionHandler
{
    RetainPtr contentView = _contentView.get();
    RefPtr page = [contentView page];
    _digitalCredentialsPicker = adoptNS([[WKDigitalCredentialsPicker alloc] initWithView:[contentView webView] page:page.get()]);
    [_digitalCredentialsPicker presentWithRequestData:requestData completionHandler:WTF::move(completionHandler)];
}

- (void)dismissDigitalCredentialsChooser:(CompletionHandler<void(bool)>&&)completionHandler
{
    if (!_digitalCredentialsPicker) {
        LOG(DigitalCredentials, "Digital credentials chooser is not presented.");
        completionHandler(false);
        return;
    }
    [_digitalCredentialsPicker dismissWithCompletionHandler:WTF::move(completionHandler)];
}
#endif // ENABLE(WEB_AUTHN)

- (void)showContactPicker:(const WebCore::ContactsRequestData&)requestData completionHandler:(CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&)completionHandler
{
#if HAVE(CONTACTSUI)
    _contactPicker = adoptNS([[WKContactPicker alloc] initWithView:[_contentView.get() webView]]);
    [_contactPicker setDelegate:self];
    [_contactPicker presentWithRequestData:requestData completionHandler:WTF::move(completionHandler)];
#else
    completionHandler(std::nullopt);
#endif
}

#if HAVE(CONTACTSUI)
- (void)contactPickerDidPresent:(WKContactPicker *)contactPicker
{
    ASSERT(_contactPicker == contactPicker);

    RetainPtr webView = [_contentView.get() webView];
    [webView _didPresentContactPicker];
}

- (void)contactPickerDidDismiss:(WKContactPicker *)contactPicker
{
    ASSERT(_contactPicker == contactPicker);

    [_contactPicker setDelegate:nil];
    _contactPicker = nil;

    RetainPtr webView = [_contentView.get() webView];
    [webView _didDismissContactPicker];
}
#endif

- (void)dismissPickersIfNeededWithReason:(WebKit::PickerDismissalReason)reason
{
    if ([_fileUploadPanel dismissIfNeededWithReason:reason])
        _fileUploadPanel = nil;

#if HAVE(SHARE_SHEET_UI)
    if ([_shareSheet dismissIfNeededWithReason:reason])
        _shareSheet = nil;
#endif

#if HAVE(CONTACTSUI)
    if ([_contactPicker dismissIfNeededWithReason:reason])
        _contactPicker = nil;
#endif
}

- (NSArray<NSString *> *)filePickerAcceptedTypeIdentifiers
{
    if (!_fileUploadPanel)
        return @[];

    return [_fileUploadPanel acceptedTypeIdentifiers];
}

- (void)dismissFilePicker
{
    [_fileUploadPanel dismissIfNeededWithReason:WebKit::PickerDismissalReason::Testing];
}

- (void)dismissContactPickerWithContacts:(NSArray *)contacts
{
#if HAVE(CONTACTSUI)
    [_contactPicker dismissWithContacts:contacts];
#endif
}

@end

#endif // PLATFORM(IOS_FAMILY)
