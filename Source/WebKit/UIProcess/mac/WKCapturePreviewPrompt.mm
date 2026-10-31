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
#import "WKCapturePreviewPrompt.h"

#if PLATFORM(MAC) && ENABLE(MEDIA_STREAM)

#import "CapturePreviewDeviceSelection.h"
#import "CapturePreviewLevelMeterState.h"
#import "CapturePreviewPromptModel.h"
#import "LayerHostingContext.h"
#import "Logging.h"
#import <WebCore/LocalizedStrings.h>
#import <algorithm>
#import <cmath>
#import <wtf/BlockPtr.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

constexpr CGFloat previewWidth = 320;
constexpr CGFloat previewHeight = 180;
constexpr CGFloat chooserElementSpacing = 8;
constexpr CGFloat sectionCornerRadius = 12;
constexpr CGFloat sectionInset = 12;
constexpr CGFloat audioLevelIconSpacing = 10;
constexpr CGFloat previewCornerRadius = 8;
constexpr CGFloat previewBadgeInset = 8;
constexpr CGFloat previewBadgeHorizontalPadding = 6;
constexpr CGFloat previewBadgeVerticalPadding = 2;
constexpr CGFloat previewBadgeCapsuleCornerRadius = 100;
constexpr CGFloat promptSheetInset = 16;
constexpr CGFloat promptSheetTopInset = 20;
constexpr CGFloat promptSheetSpacing = 16;
constexpr CGFloat promptSheetInformativeTextSpacing = 9;
constexpr CGFloat promptSheetButtonSpacing = 6;
constexpr CGFloat promptSheetMinimumContentWidth = 260;
constexpr CGFloat promptSheetMaximumContentWidth = 420;
constexpr CGFloat promptSheetMinimumScrolledHeight = 120;
constexpr CGFloat promptSheetScreenMargin = 32;

static void pinToWidthOfStack(NSView *view, NSStackView *stack)
{
    // NSStackView does not stretch its views to its own width.
    [NSLayoutConstraint activateConstraints:@[
        [[view leadingAnchor] constraintEqualToAnchor:[stack leadingAnchor]],
        [[view trailingAnchor] constraintEqualToAnchor:[stack trailingAnchor]],
    ]];
}

@interface WKCaptureDeviceChooserLevelView : NSView {
    WebKit::CapturePreviewLevelMeterState _state;
}
- (void)setLevel:(float)level;
- (void)reset;
@end

@implementation WKCaptureDeviceChooserLevelView

- (NSSize)intrinsicContentSize
{
    return NSMakeSize(NSViewNoIntrinsicMetric, WebKit::CapturePreviewLevelMeterState::height);
}

- (BOOL)isAccessibilityElement
{
    return YES;
}

- (NSAccessibilityRole)accessibilityRole
{
    return NSAccessibilityLevelIndicatorRole;
}

- (NSString *)accessibilityLabel
{
    return WEB_UI_STRING_KEY(@"Microphone level", "Microphone level (usermedia)", @"Accessibility label for the microphone level meter in the user media prompt").createNSString().autorelease();
}

- (id)accessibilityValue
{
    return @(std::round(_state.levelFraction() * 100));
}

- (NSString *)accessibilityValueDescription
{
    return [NSNumberFormatter localizedStringFromNumber:@(_state.levelFraction()) numberStyle:NSNumberFormatterPercentStyle];
}

- (id)accessibilityMinValue
{
    return @0;
}

- (id)accessibilityMaxValue
{
    return @100;
}

- (void)setLevel:(float)level
{
    if (_state.update(level, MonotonicTime::now()))
        [self setNeedsDisplay:YES];
}

- (void)reset
{
    if (_state.reset())
        [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)rect
{
    using State = WebKit::CapturePreviewLevelMeterState;
    NSRect bounds = [self bounds];
    CGFloat radius = State::segmentWidth / 2;

    for (unsigned index = 0; index < State::segmentCount; ++index) {
        switch (_state.segment(index)) {
        case State::Segment::Peak:
            [[NSColor controlAccentColor] setFill];
            break;
        case State::Segment::Lit:
            [[NSColor secondaryLabelColor] setFill];
            break;
        case State::Segment::Unlit:
            [[NSColor quaternaryLabelColor] setFill];
            break;
        }

        NSRect segment = NSMakeRect(NSMinX(bounds) + State::segmentX(index, NSWidth(bounds)), NSMinY(bounds), State::segmentWidth, NSHeight(bounds));
        [[NSBezierPath bezierPathWithRoundedRect:segment xRadius:radius yRadius:radius] fill];
    }
}

@end

// Draws its own rounded background instead of using NSBox, whose contentViewMargins position the
// content view by frame and so leave a constraint based content view unresolved.
@interface WKCaptureDeviceChooserRoundedView : NSView {
    RetainPtr<NSColor> _fillColor;
    CGFloat _cornerRadius;
}
- (instancetype)initWithFillColor:(NSColor *)fillColor cornerRadius:(CGFloat)cornerRadius;
@end

@implementation WKCaptureDeviceChooserRoundedView

- (instancetype)initWithFillColor:(NSColor *)fillColor cornerRadius:(CGFloat)cornerRadius
{
    if (!(self = [super initWithFrame:NSZeroRect]))
        return nil;

    _fillColor = fillColor;
    _cornerRadius = cornerRadius;
    [self setTranslatesAutoresizingMaskIntoConstraints:NO];

    return self;
}

- (void)drawRect:(NSRect)rect
{
    NSRect bounds = [self bounds];
    CGFloat radius = std::min(_cornerRadius, NSHeight(bounds) / 2);

    [_fillColor setFill];
    [[NSBezierPath bezierPathWithRoundedRect:bounds xRadius:radius yRadius:radius] fill];
}

@end


@implementation WKCaptureDeviceChooser {
    RetainPtr<NSView> _view;
    RetainPtr<NSPopUpButton> _videoPopUp;
    RetainPtr<NSPopUpButton> _audioPopUp;
    RetainPtr<WKCaptureDeviceChooserLevelView> _audioLevelMeter;
    RetainPtr<NSArray<NSButton *>> _allowButtons;

    RetainPtr<NSView> _previewContainer;
    RetainPtr<CALayer> _previewLayerHost;
    WebKit::LayerHostingContextID _previewHostingContextID;

    WebKit::CapturePreviewPromptModel _model;
}

- (instancetype)initWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices
{
    if (!(self = [super init]))
        return nil;

    _model = { WTF::move(videoDevices), WTF::move(audioDevices) };

    _previewContainer = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(0, 0, previewWidth, previewHeight)]);
    [_previewContainer setWantsLayer:YES];
    [[_previewContainer layer] setBackgroundColor:RetainPtr { [NSColor blackColor].CGColor }];
    [[_previewContainer layer] setCornerRadius:previewCornerRadius];
    [[_previewContainer layer] setMasksToBounds:YES];
    [_previewContainer setTranslatesAutoresizingMaskIntoConstraints:NO];

    RetainPtr contentStack = adoptNS([[NSStackView alloc] init]);
    [contentStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [contentStack setDistribution:NSStackViewDistributionFill];
    [contentStack setSpacing:chooserElementSpacing];
    [contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    if (_model.requestNeeds(WebKit::CapturePreviewDeviceType::Camera)) {
        RetainPtr badgeLabel = adoptNS([[NSTextField alloc] initWithFrame:NSZeroRect]);
        [badgeLabel setStringValue:WEB_UI_STRING_KEY(@"Preview", "Preview (usermedia)", @"Badge label identifying the live camera preview in the user media prompt").createNSString().get()];
        [badgeLabel setEditable:NO];
        [badgeLabel setSelectable:NO];
        [badgeLabel setBordered:NO];
        [badgeLabel setDrawsBackground:NO];
        [badgeLabel setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
        [badgeLabel setTextColor:[NSColor alternateSelectedControlTextColor]];
        [badgeLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

        RetainPtr badge = adoptNS([[WKCaptureDeviceChooserRoundedView alloc] initWithFillColor:[NSColor controlAccentColor] cornerRadius:previewBadgeCapsuleCornerRadius]);
        [badge addSubview:badgeLabel.get()];
        [_previewContainer addSubview:badge.get()];

        _videoPopUp = adoptNS([[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]);
        [_videoPopUp setTarget:self];
        [_videoPopUp setAction:@selector(_selectedVideoDeviceDidChange:)];
        [_videoPopUp setAccessibilityLabel:WEB_UI_STRING_KEY(@"Camera", "Camera (usermedia)", @"Accessibility label for the camera menu in the user media prompt").createNSString().get()];
        [_videoPopUp setTranslatesAutoresizingMaskIntoConstraints:NO];

        RetainPtr cameraStack = adoptNS([[NSStackView alloc] init]);
        [cameraStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [cameraStack setDistribution:NSStackViewDistributionFill];
        [cameraStack setSpacing:sectionInset];
        [cameraStack setTranslatesAutoresizingMaskIntoConstraints:NO];
        [cameraStack addArrangedSubview:_previewContainer.get()];
        [cameraStack addArrangedSubview:_videoPopUp.get()];
        pinToWidthOfStack(_previewContainer.get(), cameraStack.get());
        pinToWidthOfStack(_videoPopUp.get(), cameraStack.get());

        RetainPtr cameraSection = [self _createSectionWithContentView:cameraStack.get()];
        [contentStack addArrangedSubview:cameraSection.get()];
        pinToWidthOfStack(cameraSection.get(), contentStack.get());

        [NSLayoutConstraint activateConstraints:@[
            [[_previewContainer widthAnchor] constraintEqualToConstant:previewWidth],
            [[_previewContainer heightAnchor] constraintEqualToConstant:previewHeight],
            [[badgeLabel topAnchor] constraintEqualToAnchor:[badge topAnchor] constant:previewBadgeVerticalPadding],
            [[badgeLabel bottomAnchor] constraintEqualToAnchor:[badge bottomAnchor] constant:-previewBadgeVerticalPadding],
            [[badgeLabel leadingAnchor] constraintEqualToAnchor:[badge leadingAnchor] constant:previewBadgeHorizontalPadding],
            [[badgeLabel trailingAnchor] constraintEqualToAnchor:[badge trailingAnchor] constant:-previewBadgeHorizontalPadding],
            [[badge topAnchor] constraintEqualToAnchor:[_previewContainer topAnchor] constant:previewBadgeInset],
            [[badge leadingAnchor] constraintEqualToAnchor:[_previewContainer leadingAnchor] constant:previewBadgeInset],
        ]];
    }

    if (_model.requestNeeds(WebKit::CapturePreviewDeviceType::Microphone)) {
        RetainPtr microphoneIcon = adoptNS([[NSImageView alloc] init]);
        [microphoneIcon setImage:[NSImage imageWithSystemSymbolName:@"mic" accessibilityDescription:nil]];
        [microphoneIcon setContentTintColor:[NSColor secondaryLabelColor]];
        [microphoneIcon setAccessibilityElement:NO];
        [microphoneIcon setTranslatesAutoresizingMaskIntoConstraints:NO];
        [microphoneIcon setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];

        _audioLevelMeter = adoptNS([[WKCaptureDeviceChooserLevelView alloc] initWithFrame:NSZeroRect]);
        [_audioLevelMeter setTranslatesAutoresizingMaskIntoConstraints:NO];

        RetainPtr meterRow = adoptNS([[NSStackView alloc] init]);
        [meterRow setOrientation:NSUserInterfaceLayoutOrientationHorizontal];
        [meterRow setDistribution:NSStackViewDistributionFill];
        [meterRow setAlignment:NSLayoutAttributeCenterY];
        [meterRow setSpacing:audioLevelIconSpacing];
        [meterRow setTranslatesAutoresizingMaskIntoConstraints:NO];
        [meterRow addArrangedSubview:microphoneIcon.get()];
        [meterRow addArrangedSubview:_audioLevelMeter.get()];

        _audioPopUp = adoptNS([[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]);
        [_audioPopUp setTarget:self];
        [_audioPopUp setAction:@selector(_selectedAudioDeviceDidChange:)];
        [_audioPopUp setAccessibilityLabel:WEB_UI_STRING_KEY(@"Microphone", "Microphone (usermedia)", @"Accessibility label for the microphone menu in the user media prompt").createNSString().get()];
        [_audioPopUp setTranslatesAutoresizingMaskIntoConstraints:NO];

        RetainPtr microphoneStack = adoptNS([[NSStackView alloc] init]);
        [microphoneStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [microphoneStack setDistribution:NSStackViewDistributionFill];
        [microphoneStack setSpacing:sectionInset];
        [microphoneStack setTranslatesAutoresizingMaskIntoConstraints:NO];
        [microphoneStack addArrangedSubview:meterRow.get()];
        [microphoneStack addArrangedSubview:_audioPopUp.get()];
        pinToWidthOfStack(meterRow.get(), microphoneStack.get());
        pinToWidthOfStack(_audioPopUp.get(), microphoneStack.get());

        RetainPtr microphoneSection = [self _createSectionWithContentView:microphoneStack.get()];
        [contentStack addArrangedSubview:microphoneSection.get()];
        pinToWidthOfStack(microphoneSection.get(), contentStack.get());
    }

    _view = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [_view addSubview:contentStack.get()];

    [NSLayoutConstraint activateConstraints:@[
        [[contentStack topAnchor] constraintEqualToAnchor:[_view topAnchor]],
        [[contentStack bottomAnchor] constraintEqualToAnchor:[_view bottomAnchor]],
        [[contentStack centerXAnchor] constraintEqualToAnchor:[_view centerXAnchor]],
        [[contentStack leadingAnchor] constraintGreaterThanOrEqualToAnchor:[_view leadingAnchor]],
        [[contentStack trailingAnchor] constraintLessThanOrEqualToAnchor:[_view trailingAnchor]],
    ]];
    [_view layoutSubtreeIfNeeded];
    [_view setFrameSize:[_view fittingSize]];

    [self _rebuildMenus];

    return self;
}

- (NSView *)_createSectionWithContentView:(NSView *)contentView
{
    RetainPtr section = adoptNS([[WKCaptureDeviceChooserRoundedView alloc] initWithFillColor:[NSColor quaternarySystemFillColor] cornerRadius:sectionCornerRadius]);
    [section addSubview:contentView];
    [NSLayoutConstraint activateConstraints:@[
        [[contentView topAnchor] constraintEqualToAnchor:[section topAnchor] constant:sectionInset],
        [[contentView bottomAnchor] constraintEqualToAnchor:[section bottomAnchor] constant:-sectionInset],
        [[contentView leadingAnchor] constraintEqualToAnchor:[section leadingAnchor] constant:sectionInset],
        [[contentView trailingAnchor] constraintEqualToAnchor:[section trailingAnchor] constant:-sectionInset],
    ]];
    return section.autorelease();
}

- (void)dealloc
{
    [self stop];
    [super dealloc];
}

- (NSView *)view
{
    return _view.get();
}

- (NSString *)selectedVideoDeviceID
{
    return nsStringNilIfEmpty(_model.selectedDeviceID(WebKit::CapturePreviewDeviceType::Camera)).autorelease();
}

- (NSString *)selectedAudioDeviceID
{
    return nsStringNilIfEmpty(_model.selectedDeviceID(WebKit::CapturePreviewDeviceType::Microphone)).autorelease();
}

- (void)_rebuildMenu:(NSPopUpButton *)popUp type:(WebKit::CapturePreviewDeviceType)type
{
    if (!popUp)
        return;

    // Menu items are built directly because `-addItemWithTitle:` drops duplicate titles, and multiple
    // devices can have the same label.
    auto& devices = _model.devices(type);
    RetainPtr menu = adoptNS([[NSMenu alloc] init]);
    for (auto& device : devices) {
        RetainPtr title = device.label().createNSString();
        RetainPtr item = adoptNS([[NSMenuItem alloc] initWithTitle:title.get() action:nil keyEquivalent:@""]);
        [item setToolTip:title.get()];
        [menu addItem:item.get()];
    }

    [popUp setMenu:menu.get()];

    if (!devices.isEmpty())
        [popUp selectItemAtIndex:_model.selectedIndex(type)];

    [popUp setHidden:devices.isEmpty()];
    [popUp setEnabled:devices.size() > 1];
}

- (void)_rebuildMenus
{
    [self _rebuildMenu:_videoPopUp.get() type:WebKit::CapturePreviewDeviceType::Camera];
    [self _rebuildMenu:_audioPopUp.get() type:WebKit::CapturePreviewDeviceType::Microphone];
}

- (void)setAllowButtons:(NSArray<NSButton *> *)allowButtons
{
    _allowButtons = allowButtons;
    [self _updateAllowButtonEnablement];
}

- (void)_updateAllowButtonEnablement
{
    bool canStillSatisfyRequest = _model.canStillSatisfyRequest();
    [_allowButtons enumerateObjectsUsingBlock:^(NSButton *allowButton, NSUInteger, BOOL *) {
        [allowButton setEnabled:canStillSatisfyRequest];
    }];
}

- (void)updateWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices
{
    if (_model.hasSameDevices(videoDevices, audioDevices))
        return;

    String previousMicrophoneID = _model.selectedDeviceID(WebKit::CapturePreviewDeviceType::Microphone);
    bool previewNeedsRestart = _model.update(WTF::move(videoDevices), WTF::move(audioDevices));
    [self _rebuildMenus];
    [self _updateAllowButtonEnablement];

    if (_model.selectedDeviceID(WebKit::CapturePreviewDeviceType::Microphone) != previousMicrophoneID)
        [_audioLevelMeter reset];

    if (previewNeedsRestart)
        _model.notifySelectionChanged();
}

- (void)setPreviewHostingContext:(const WebCore::HostingContext&)hostingContext gpuProcessIdentifier:(ProcessID)gpuProcessIdentifier
{
    UNUSED_PARAM(gpuProcessIdentifier);
    if (_model.shouldIgnoreHostingContext(hostingContext))
        return;
    if (_previewLayerHost && hostingContext.contextID == _previewHostingContextID)
        return;

    _previewHostingContextID = hostingContext.contextID;

    [_previewLayerHost removeFromSuperlayer];
    _previewLayerHost = nil;

    if (!hostingContext.contextID)
        return;

    _previewLayerHost = WebKit::LayerHostingContext::createPlatformLayerForHostingContext(hostingContext.contextID);
    if (!_previewLayerHost) {
        RELEASE_LOG_ERROR(WebRTC, "CapturePreview unable to create a hosting layer for contextID=%u", hostingContext.contextID);
        return;
    }

    [_previewLayerHost setFrame:[_previewContainer bounds]];
    [_previewLayerHost setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];
    [[_previewContainer layer] insertSublayer:_previewLayerHost.get() atIndex:0];
}

- (void)setSelectionChangedHandler:(Function<void(std::optional<WebCore::CaptureDevice>&&, std::optional<WebCore::CaptureDevice>&&)>&&)handler
{
    _model.setSelectionChangedHandler(WTF::move(handler));
}

- (BOOL)isStopped
{
    return _model.isStopped();
}

- (CGSize)previewSize
{
    return CGSizeMake(previewWidth, previewHeight);
}

- (void)setAudioLevel:(float)level
{
    [_audioLevelMeter setLevel:level];
}

- (void)beginPreview
{
    _model.beginPreview();
}

- (void)_popUp:(NSPopUpButton *)popUp didSelectDeviceOfType:(WebKit::CapturePreviewDeviceType)type
{
    NSInteger index = [popUp indexOfSelectedItem];
    if (index < 0 || static_cast<size_t>(index) >= _model.devices(type).size())
        return;

    String previousDeviceID = _model.selectedDeviceID(type);
    _model.select(type, index);
    if (type == WebKit::CapturePreviewDeviceType::Microphone && _model.selectedDeviceID(type) != previousDeviceID)
        [_audioLevelMeter reset];
    _model.notifySelectionChanged();
}

- (void)_selectedVideoDeviceDidChange:(id)sender
{
    [self _popUp:_videoPopUp.get() didSelectDeviceOfType:WebKit::CapturePreviewDeviceType::Camera];
}

- (void)_selectedAudioDeviceDidChange:(id)sender
{
    [self _popUp:_audioPopUp.get() didSelectDeviceOfType:WebKit::CapturePreviewDeviceType::Microphone];
}

- (void)stop
{
    _model.stop();
    [self setPreviewHostingContext:WebCore::HostingContext { } gpuProcessIdentifier:0];
    [_audioLevelMeter reset];
}

@end


@implementation WKCapturePreviewPromptSheet {
    RetainPtr<NSString> _messageText;
    RetainPtr<NSString> _informativeText;
    RetainPtr<NSView> _accessoryView;
    RetainPtr<NSStackView> _buttonStack;
    RetainPtr<NSPanel> _window;
    WeakObjCPtr<NSWindow> _parentWindow;
    BlockPtr<void()> _didPresentHandler;
    BlockPtr<void(NSModalResponse)> _completionHandler;
    RetainPtr<id> _occlusionObserver;
    CGFloat _widestButtonWidth;
}

- (instancetype)initWithMessageText:(NSString *)messageText informativeText:(NSString *)informativeText accessoryView:(NSView *)accessoryView
{
    if (!(self = [super init]))
        return nil;

    _messageText = messageText;
    _informativeText = informativeText;
    _accessoryView = accessoryView;

    _buttonStack = adoptNS([[NSStackView alloc] init]);
    [_buttonStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [_buttonStack setDistribution:NSStackViewDistributionFill];
    [_buttonStack setSpacing:promptSheetButtonSpacing];
    [_buttonStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    return self;
}

- (NSButton *)addButtonWithTitle:(NSString *)title response:(NSModalResponse)response
{
    RetainPtr button = [NSButton buttonWithTitle:title target:self action:@selector(_buttonClicked:)];
    [button setTag:response];
    [button setKeyEquivalent:@""];
    [button setControlSize:NSControlSizeLarge];
    [button setToolTip:title];
    [button setTranslatesAutoresizingMaskIntoConstraints:NO];
    _widestButtonWidth = std::max(_widestButtonWidth, [button fittingSize].width);
    [_buttonStack addArrangedSubview:button.get()];
    pinToWidthOfStack(button.get(), _buttonStack.get());
    return button.autorelease();
}

- (void)dealloc
{
    [self _stopObservingPresentation];
    [super dealloc];
}

- (void)beginSheetModalForWindow:(NSWindow *)parentWindow didPresentHandler:(void (^)(void))didPresentHandler completionHandler:(void (^)(NSModalResponse))completionHandler
{
    CGFloat contentWidth = std::max({ promptSheetMinimumContentWidth, NSWidth([_accessoryView frame]), std::min(_widestButtonWidth, promptSheetMaximumContentWidth) });

    RetainPtr<NSMutableArray<NSView *>> arrangedViews = adoptNS([[NSMutableArray alloc] init]);

    RetainPtr messageLabel = [NSTextField wrappingLabelWithString:_messageText.get()];
    [messageLabel setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
    [messageLabel setSelectable:NO];
    [messageLabel setPreferredMaxLayoutWidth:contentWidth];
    [messageLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [arrangedViews addObject:messageLabel.get()];

    if (_informativeText) {
        RetainPtr informativeLabel = [NSTextField wrappingLabelWithString:_informativeText.get()];
        [informativeLabel setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
        [informativeLabel setSelectable:NO];
        [informativeLabel setPreferredMaxLayoutWidth:contentWidth];
        [informativeLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
        [arrangedViews addObject:informativeLabel.get()];
    }

    [_accessoryView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [arrangedViews addObject:_accessoryView.get()];

    RetainPtr scrolledStack = adoptNS([[NSStackView alloc] init]);
    [scrolledStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [scrolledStack setDistribution:NSStackViewDistributionFill];
    [scrolledStack setSpacing:promptSheetSpacing];
    [scrolledStack setTranslatesAutoresizingMaskIntoConstraints:NO];
    for (NSView *view in arrangedViews.get()) {
        [scrolledStack addArrangedSubview:view];
        pinToWidthOfStack(view, scrolledStack.get());
    }
    if (_informativeText)
        [scrolledStack setCustomSpacing:promptSheetInformativeTextSpacing afterView:messageLabel.get()];

    RetainPtr scrollView = adoptNS([[NSScrollView alloc] initWithFrame:NSZeroRect]);
    [scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutohidesScrollers:YES];
    [scrollView setDrawsBackground:NO];
    [scrollView setDocumentView:scrolledStack.get()];
    RetainPtr clipView = [scrollView contentView];
    [NSLayoutConstraint activateConstraints:@[
        [[scrolledStack topAnchor] constraintEqualToAnchor:[clipView topAnchor]],
        [[scrolledStack leadingAnchor] constraintEqualToAnchor:[clipView leadingAnchor]],
        [[scrolledStack widthAnchor] constraintEqualToConstant:contentWidth],
    ]];
    [scrolledStack layoutSubtreeIfNeeded];

    RetainPtr screen = [parentWindow screen] ?: [NSScreen mainScreen];
    NSRect visibleFrame = [screen visibleFrame];
    CGFloat sheetTop = std::min(NSMaxY([parentWindow convertRectToScreen:[parentWindow contentLayoutRect]]), NSMaxY(visibleFrame));
    CGFloat availableHeight = sheetTop - NSMinY(visibleFrame) - promptSheetTopInset - promptSheetInset - promptSheetScreenMargin - promptSheetSpacing - [_buttonStack fittingSize].height;
    CGFloat scrolledHeight = std::min([scrolledStack fittingSize].height, std::max(availableHeight, promptSheetMinimumScrolledHeight));
    [[[scrollView heightAnchor] constraintEqualToConstant:scrolledHeight] setActive:YES];

    RetainPtr contentStack = adoptNS([[NSStackView alloc] init]);
    [contentStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [contentStack setDistribution:NSStackViewDistributionFill];
    [contentStack setSpacing:promptSheetSpacing];
    [contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];
    for (NSView *view in @[ scrollView.get(), _buttonStack.get() ]) {
        [contentStack addArrangedSubview:view];
        pinToWidthOfStack(view, contentStack.get());
    }

    RetainPtr contentView = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    [contentView addSubview:contentStack.get()];
    [NSLayoutConstraint activateConstraints:@[
        [[contentStack topAnchor] constraintEqualToAnchor:[contentView topAnchor] constant:promptSheetTopInset],
        [[contentStack bottomAnchor] constraintEqualToAnchor:[contentView bottomAnchor] constant:-promptSheetInset],
        [[contentStack leadingAnchor] constraintEqualToAnchor:[contentView leadingAnchor] constant:promptSheetInset],
        [[contentStack trailingAnchor] constraintEqualToAnchor:[contentView trailingAnchor] constant:-promptSheetInset],
        [[contentStack widthAnchor] constraintEqualToConstant:contentWidth],
    ]];

    _window = adoptNS([[NSPanel alloc] initWithContentRect:NSZeroRect styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:YES]);
    [_window setReleasedWhenClosed:NO];
    [_window setContentView:contentView.get()];
    [contentView layoutSubtreeIfNeeded];
    [_window setContentSize:[contentView fittingSize]];

    _parentWindow = parentWindow;
    _didPresentHandler = makeBlockPtr(didPresentHandler);
    _occlusionObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidChangeOcclusionStateNotification object:_window.get() queue:nil usingBlock:makeBlockPtr([weakSelf = WeakObjCPtr<WKCapturePreviewPromptSheet>(self)](NSNotification *) {
        [weakSelf.get() _presentationMayHaveChanged];
    }).get()];

    _completionHandler = makeBlockPtr(completionHandler);
    [parentWindow beginSheet:_window.get() completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKCapturePreviewPromptSheet>(self)](NSModalResponse returnCode) {
        [weakSelf.get() _finishWithResponse:returnCode];
    }).get()];

    [self _presentationMayHaveChanged];
}

- (void)_presentationMayHaveChanged
{
    if (!_didPresentHandler || !([_window occlusionState] & NSWindowOcclusionStateVisible))
        return;

    auto didPresentHandler = std::exchange(_didPresentHandler, nullptr);
    [self _stopObservingPresentation];
    didPresentHandler();
}

- (void)_stopObservingPresentation
{
    _didPresentHandler = nullptr;
    if (RetainPtr observer = std::exchange(_occlusionObserver, nil))
        [[NSNotificationCenter defaultCenter] removeObserver:observer.get()];
}

- (void)_finishWithResponse:(NSModalResponse)response
{
    [self _stopObservingPresentation];
    if (auto completionHandler = std::exchange(_completionHandler, nullptr))
        completionHandler(response);
}

- (void)dismissWithResponse:(NSModalResponse)response
{
    if (!_completionHandler)
        return;

    RetainPtr parentWindow = [_window sheetParent] ?: _parentWindow.get().get();
    if (parentWindow) {
        [parentWindow endSheet:_window.get() returnCode:response];
        return;
    }

    [_window orderOut:nil];
    [self _finishWithResponse:response];
}

- (void)_buttonClicked:(NSButton *)sender
{
    [[_window sheetParent] endSheet:_window.get() returnCode:[sender tag]];
}

@end

#endif // PLATFORM(MAC) && ENABLE(MEDIA_STREAM)
