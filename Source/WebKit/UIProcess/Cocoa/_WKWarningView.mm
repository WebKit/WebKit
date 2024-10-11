/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "_WKWarningView.h"

#import "BrowsingWarning.h"
#import "PageClient.h"
#import <WebCore/FontCocoa.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/Language.h>
#import <wtf/MainThread.h>
#import <wtf/URL.h>

#if PLATFORM(WATCHOS)
#import "PepperUICoreSPI.h"
#endif

constexpr CGFloat boxCornerRadius = 6;
#if HAVE(SAFE_BROWSING)
#if PLATFORM(WATCHOS)
constexpr CGFloat marginSize = 9;
#else
constexpr CGFloat marginSize = 20;
constexpr CGFloat warningSymbolPointSize = 30;
#endif
constexpr CGFloat maxWidth = 675;
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
constexpr CGFloat topToBottomMarginMultiplier = 0.2;
#else
constexpr CGFloat topToBottomMarginMultiplier = 0.5;
#endif
#endif

constexpr CGFloat imageIconPointSizeMultiplier = 0.9;

#if PLATFORM(MAC)
using TextViewType = NSTextView;
using ButtonType = NSButton;
using AlignmentType = NSLayoutAttribute;
using SizeType = NSSize;
#else
using TextViewType = UITextView;
using ButtonType = UIButton;
using AlignmentType = UIStackViewAlignment;
using SizeType = CGSize;
#endif

enum class WarningItem : uint8_t {
    BrowsingWarningBackground,
    BoxBackground,
    WarningSymbol,
    TitleText,
    MessageText,
    ShowDetailsButton,
    GoBackButton,
    ContinueButton
};

enum class WarningTextSize : uint8_t {
    Title,
    Body
};

static WebCore::CocoaFont *fontOfSize(WarningTextSize size)
{
#if PLATFORM(MAC)
    switch (size) {
    case WarningTextSize::Title:
        return [NSFont boldSystemFontOfSize:26];
    case WarningTextSize::Body:
        return [NSFont systemFontOfSize:14];
    }
#elif HAVE(SAFE_BROWSING)
    switch (size) {
    case WarningTextSize::Title:
#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
        return [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
#else
        return [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
#endif
    case WarningTextSize::Body:
        return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    }
#else
    return nil;
#endif
}

static WebCore::CocoaColor *colorForItem(WarningItem item, ViewType *warning)
{
    ASSERT([warning isKindOfClass:[_WKWarningView class]]);
    _WKWarningView *warningView = (_WKWarningView *)warning;
#if PLATFORM(MAC)

    auto colorNamed = [] (NSString *name) -> WebCore::CocoaColor * {
#if HAVE(SAFE_BROWSING)
        return [NSColor colorNamed:name bundle:[NSBundle bundleForClass:NSClassFromString(@"WKWebView")]];
#else
        ASSERT_NOT_REACHED();
        return nil;
#endif
    };

    switch (item) {
    case WarningItem::BrowsingWarningBackground:
        return WTF::switchOn(warningView.warning->data(), [&colorNamed] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return colorNamed(@"WKSafeBrowsingWarningBackground");
        }, [&colorNamed] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return colorNamed(@"WKHTTPSWarningBackground");
        });
    case WarningItem::BoxBackground:
        return WTF::switchOn(warningView.warning->data(), [] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return [NSColor windowBackgroundColor];
        }, [&colorNamed] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return colorNamed(@"WKHTTPSWarningBoxBackground");
        });
    case WarningItem::TitleText:
    case WarningItem::WarningSymbol:
        return WTF::switchOn(warningView.warning->data(), [&colorNamed] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return colorNamed(@"WKSafeBrowsingWarningTitle");
        }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return item == WarningItem::WarningSymbol ? NSColor.redColor : colorNamed(@"WKHTTPSWarningTitle");
        });
    case WarningItem::MessageText:
        return WTF::switchOn(warningView.warning->data(), [&colorNamed] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return colorNamed(@"WKSafeBrowsingWarningText");
        }, [&colorNamed] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return colorNamed(@"WKHTTPSWarningText");
        });
    case WarningItem::ShowDetailsButton:
    case WarningItem::GoBackButton:
    case WarningItem::ContinueButton:
        ASSERT_NOT_REACHED();
        return nil;
    }
#else
    UIColor *red = [UIColor colorWithRed:0.998 green:0.239 blue:0.233 alpha:1.0];
    UIColor *darkGray = [UIColor colorWithRed:0.118 green:0.118 blue:0.118 alpha:1.0];
    UIColor *lighterGray = [UIColor colorWithRed:0.937 green:0.937 blue:0.937 alpha:1.0];
    UIColor *white = [UIColor whiteColor];

    bool narrow = warning.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;

    switch (item) {
    case WarningItem::BrowsingWarningBackground:
        return WTF::switchOn(warningView.warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return red;
        }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return lighterGray;
        });
    case WarningItem::BoxBackground:
        return WTF::switchOn(warningView.warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return narrow ? red : white;
        }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return lighterGray;
        });
    case WarningItem::TitleText:
    case WarningItem::WarningSymbol:
        return WTF::switchOn(warningView.warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return narrow ? white : red;
        }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return item == WarningItem::TitleText ? darkGray : red;
        });
    case WarningItem::MessageText:
    case WarningItem::ShowDetailsButton:
    case WarningItem::ContinueButton:
        return WTF::switchOn(warningView.warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return narrow ? white : [UIColor darkTextColor];
        }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return darkGray;
        });
    case WarningItem::GoBackButton:
        return WTF::switchOn(warningView.warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
            return narrow ? white : warning.tintColor;
        }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
            return narrow ? darkGray : warning.tintColor;
        });
    }
#endif
    ASSERT_NOT_REACHED();
    return nil;
}

static RetainPtr<ViewType> viewForIconImage(_WKWarningView *warningView)
{
    NSString *symbolName;
    WebCore::CocoaColor *color = colorForItem(WarningItem::WarningSymbol, warningView);
    BOOL shouldSetTint = NO;
    CGFloat imagePointSize = fontOfSize(WarningTextSize::Title).pointSize * imageIconPointSizeMultiplier;
    WTF::switchOn(warningView.warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
        symbolName = @"exclamationmark.circle.fill";
    }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
        symbolName = @"lock.slash.fill";
        shouldSetTint = YES;
    });
#if PLATFORM(MAC)
    RetainPtr view = [NSImageView imageViewWithImage:[NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:nil]];
    [view setSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:imagePointSize weight:NSFontWeightRegular scale:NSImageSymbolScaleLarge]];
    if (shouldSetTint)
        [view setContentTintColor:color];
#else
    RetainPtr view = adoptNS([[UIImageView alloc] initWithImage:[UIImage systemImageNamed:symbolName]]);
    [view setTintColor:color];
    [view setPreferredSymbolConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:imagePointSize]];
    [view setContentMode:UIViewContentModeScaleAspectFit];
#endif
    return view;
}

static ButtonType *makeButton(WarningItem item, _WKWarningView *warning, SEL action)
{
    NSString *title = nil;
    if (item == WarningItem::ShowDetailsButton)
        title = WEB_UI_NSSTRING(@"Show Details", "Action from safe browsing warning");
    else if (item == WarningItem::ContinueButton)
        title = WEB_UI_NSSTRING(@"Continue", "Continue");
    else
        title = WEB_UI_NSSTRING(@"Go Back", "Action from safe browsing warning");
#if PLATFORM(MAC)
    return [NSButton buttonWithTitle:title target:warning action:action];
#else
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    auto attributedTitle = adoptNS([[NSAttributedString alloc] initWithString:title attributes:@{
        NSUnderlineStyleAttributeName:@(NSUnderlineStyleSingle),
        NSUnderlineColorAttributeName:[UIColor whiteColor],
        NSForegroundColorAttributeName:colorForItem(item, warning),
        NSFontAttributeName:fontOfSize(WarningTextSize::Body)
    }]);
    [button setAttributedTitle:attributedTitle.get() forState:UIControlStateNormal];
    [button addTarget:warning action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
#endif
}

#if HAVE(SAFE_BROWSING)
static CGSize buttonSize(ButtonType *button)
{
#if PLATFORM(MAC)
    return button.frame.size;
#else
    return button.titleLabel.intrinsicContentSize;
#endif
}
#endif

static RetainPtr<ViewType> makeLabel(NSAttributedString *attributedString)
{
#if PLATFORM(MAC)
    return [NSTextField labelWithAttributedString:attributedString];
#else
    auto label = adoptNS([UILabel new]);
    [label setAttributedText:attributedString];
    [label setLineBreakMode:NSLineBreakByWordWrapping];
    [label setNumberOfLines:0];
    [label setAccessibilityTraits:UIAccessibilityTraitHeader];
    return label;
#endif
}

@implementation _WKWarningViewBox

- (void)setWarningViewBackgroundColor:(WebCore::CocoaColor *)color
{
#if PLATFORM(MAC)
    _backgroundColor = color;
    self.wantsLayer = YES;
#else
    self.backgroundColor = color;
#endif
}

#if PLATFORM(MAC)
- (void)updateLayer
{
    self.layer.backgroundColor = [_backgroundColor CGColor];
}
#endif

@end

@interface _WKWarningViewTextView : TextViewType {
@package
    WeakObjCPtr<_WKWarningView> _warning;
}
- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(_WKWarningView *)warning;
@end

@implementation _WKWarningView

- (instancetype)initWithFrame:(RectType)frame browsingWarning:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    if (!(self = [super initWithFrame:frame])) {
        completionHandler(WebKit::ContinueUnsafeLoad::Yes);
        return nil;
    }
    _completionHandler = [weakSelf = WeakObjCPtr<_WKWarningView>(self), completionHandler = WTFMove(completionHandler)] (std::variant<WebKit::ContinueUnsafeLoad, URL>&& result) mutable {
#if PLATFORM(WATCHOS)
        if (auto strongSelf = weakSelf.get())
            [strongSelf.get()->_previousFirstResponder becomeFirstResponder];
#endif
        completionHandler(WTFMove(result));
    };
    _warning = &warning;
#if PLATFORM(MAC)
    [self setWarningViewBackgroundColor:colorForItem(WarningItem::BrowsingWarningBackground, self)];
    [self addContent];
#else
    [self setBackgroundColor:colorForItem(WarningItem::BrowsingWarningBackground, self)];
#endif

#if PLATFORM(WATCHOS)
    self.crownInputScrollDirection = PUICCrownInputScrollDirectionVertical;
#endif
    return self;
}

- (void)addContent
{
    RetainPtr warningViewIcon = viewForIconImage(self);
    auto title = makeLabel(adoptNS([[NSAttributedString alloc] initWithString:_warning->title() attributes:@{
        NSFontAttributeName:fontOfSize(WarningTextSize::Title),
        NSForegroundColorAttributeName:colorForItem(WarningItem::TitleText, self)
#if PLATFORM(WATCHOS)
        , NSHyphenationFactorDocumentAttribute:@1
#endif
    }]).get());
    auto warning = makeLabel(adoptNS([[NSAttributedString alloc] initWithString:_warning->warning() attributes:@{
        NSFontAttributeName:fontOfSize(WarningTextSize::Body),
        NSForegroundColorAttributeName:colorForItem(WarningItem::MessageText, self)
#if PLATFORM(WATCHOS)
        , NSHyphenationFactorDocumentAttribute:@1
#endif
    }]).get());

    auto primaryButton = WTF::switchOn(_warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
        return makeButton(WarningItem::ShowDetailsButton, self, @selector(showDetailsClicked));
    }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
        return makeButton(WarningItem::ContinueButton, self, @selector(continueClicked));
    });
    auto goBack = makeButton(WarningItem::GoBackButton, self, @selector(goBackClicked));
    auto box = adoptNS([_WKWarningViewBox new]);
    _box = box.get();
    [box setWarningViewBackgroundColor:colorForItem(WarningItem::BoxBackground, self)];
    [box layer].cornerRadius = boxCornerRadius;

    for (ViewType *view in @[ warningViewIcon.get(), title.get(), warning.get(), goBack, primaryButton ]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [box addSubview:view];
    }
    [box setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:box.get()];

    [NSLayoutConstraint activateConstraints:@[
#if HAVE(SAFE_BROWSING)
#if PLATFORM(WATCHOS)
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[warningViewIcon leadingAnchor]] constraintEqualToAnchor:[[warningViewIcon trailingAnchor] anchorWithOffsetToAnchor:[box trailingAnchor]]],
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[title leadingAnchor]] constraintEqualToConstant:marginSize],
        [[[warningViewIcon bottomAnchor] anchorWithOffsetToAnchor:[title topAnchor]] constraintEqualToConstant:marginSize],
        [[[box topAnchor] anchorWithOffsetToAnchor:[warningViewIcon topAnchor]] constraintEqualToConstant:marginSize + self.frame.size.height / 2],
#else
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[warningViewIcon leadingAnchor]] constraintEqualToConstant:marginSize],
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[title leadingAnchor]] constraintEqualToConstant:marginSize * 1.5 + warningSymbolPointSize],
        [[[title topAnchor] anchorWithOffsetToAnchor:[warningViewIcon topAnchor]] constraintEqualToAnchor:[[warningViewIcon bottomAnchor] anchorWithOffsetToAnchor:[title bottomAnchor]]],
        [[[box topAnchor] anchorWithOffsetToAnchor:[title topAnchor]] constraintEqualToConstant:marginSize],
#endif
        [[[title bottomAnchor] anchorWithOffsetToAnchor:[warning topAnchor]] constraintEqualToConstant:marginSize],
        [[self.topAnchor anchorWithOffsetToAnchor:[box topAnchor]] constraintEqualToAnchor:[[box bottomAnchor] anchorWithOffsetToAnchor:self.bottomAnchor] multiplier:topToBottomMarginMultiplier],
#endif // HAVE(SAFE_BROWSING)
    ]];

#if HAVE(SAFE_BROWSING)
    [NSLayoutConstraint activateConstraints:@[
        [[self.leftAnchor anchorWithOffsetToAnchor:[box leftAnchor]] constraintEqualToAnchor:[[box rightAnchor] anchorWithOffsetToAnchor:self.rightAnchor]],

        [[box widthAnchor] constraintLessThanOrEqualToConstant:maxWidth],
        [[box widthAnchor] constraintLessThanOrEqualToAnchor:self.widthAnchor],

        [[[box leadingAnchor] anchorWithOffsetToAnchor:[warning leadingAnchor]] constraintEqualToConstant:marginSize],

        [[[title trailingAnchor] anchorWithOffsetToAnchor:[box trailingAnchor]] constraintGreaterThanOrEqualToConstant:marginSize],
        [[[warning trailingAnchor] anchorWithOffsetToAnchor:[box trailingAnchor]] constraintGreaterThanOrEqualToConstant:marginSize],
        [[goBack.trailingAnchor anchorWithOffsetToAnchor:[box trailingAnchor]] constraintEqualToConstant:marginSize],

        [[[warning bottomAnchor] anchorWithOffsetToAnchor:goBack.topAnchor] constraintEqualToConstant:marginSize],
    ]];

    bool needsVerticalButtonLayout = buttonSize(primaryButton).width + buttonSize(goBack).width + 3 * marginSize > self.frame.size.width;
    if (needsVerticalButtonLayout) {
        [NSLayoutConstraint activateConstraints:@[
            [[primaryButton.trailingAnchor anchorWithOffsetToAnchor:[box trailingAnchor]] constraintEqualToConstant:marginSize],
            [[goBack.bottomAnchor anchorWithOffsetToAnchor:primaryButton.topAnchor] constraintEqualToConstant:marginSize],
            [[goBack.bottomAnchor anchorWithOffsetToAnchor:[box bottomAnchor]] constraintEqualToConstant:marginSize * 2 + buttonSize(primaryButton).height],
        ]];
    } else {
        [NSLayoutConstraint activateConstraints:@[
            [[primaryButton.trailingAnchor anchorWithOffsetToAnchor:goBack.leadingAnchor] constraintEqualToConstant:marginSize],
            [goBack.topAnchor constraintEqualToAnchor:primaryButton.topAnchor],
            [[goBack.bottomAnchor anchorWithOffsetToAnchor:[box bottomAnchor]] constraintEqualToConstant:marginSize],
        ]];
    }
#if !PLATFORM(MAC)
    [self updateContentSize];
#endif
#endif

#if PLATFORM(WATCHOS)
    self->_previousFirstResponder = [self firstResponder];
    [self becomeFirstResponder];
#endif
}

- (void)showDetailsClicked
{
    _WKWarningViewBox *box = _box.get().get();
    ButtonType *showDetails = box.subviews.lastObject;
    [showDetails removeFromSuperview];

    auto text = adoptNS([self._protectedWarning->details() mutableCopy]);
    [text addAttributes:@{ NSFontAttributeName:fontOfSize(WarningTextSize::Body) } range:NSMakeRange(0, [text length])];
    auto details = adoptNS([[_WKWarningViewTextView alloc] initWithAttributedString:text.get() forWarning:self]);
    _details = details.get();
    auto bottom = adoptNS([_WKWarningViewBox new]);
    [bottom setWarningViewBackgroundColor:colorForItem(WarningItem::BoxBackground, self)];
    [bottom layer].cornerRadius = boxCornerRadius;

#if HAVE(SAFE_BROWSING)
    constexpr auto maxY = kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
    constexpr auto minY = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
#if PLATFORM(MAC)
    box.layer.maskedCorners = maxY;
    [bottom layer].maskedCorners = minY;
#else
    box.layer.maskedCorners = minY;
    [bottom layer].maskedCorners = maxY;
#endif
#endif

    auto line = adoptNS([_WKWarningViewBox new]);
    [line setWarningViewBackgroundColor:[WebCore::CocoaColor lightGrayColor]];
    for (ViewType *view in @[details.get(), bottom.get(), line.get()])
        view.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:bottom.get()];
    [bottom addSubview:line.get()];
    [bottom addSubview:details.get()];
#if HAVE(SAFE_BROWSING)
    [NSLayoutConstraint activateConstraints:@[
        [box.widthAnchor constraintEqualToAnchor:[bottom widthAnchor]],
        [box.bottomAnchor constraintEqualToAnchor:[bottom topAnchor]],
        [box.leadingAnchor constraintEqualToAnchor:[bottom leadingAnchor]],
        [[line widthAnchor] constraintEqualToAnchor:[bottom widthAnchor]],
        [[line leadingAnchor] constraintEqualToAnchor:[bottom leadingAnchor]],
        [[line topAnchor] constraintEqualToAnchor:[bottom topAnchor]],
        [[line heightAnchor] constraintEqualToConstant:1],
        [[[bottom topAnchor] anchorWithOffsetToAnchor:[details topAnchor]] constraintEqualToConstant:marginSize],
        [[[details bottomAnchor] anchorWithOffsetToAnchor:[bottom bottomAnchor]] constraintEqualToConstant:marginSize],
        [[[bottom leadingAnchor] anchorWithOffsetToAnchor:[details leadingAnchor]] constraintEqualToConstant:marginSize],
        [[[details trailingAnchor] anchorWithOffsetToAnchor:[bottom trailingAnchor]] constraintEqualToConstant:marginSize],
    ]];
#endif
    [self layoutText];
#if !PLATFORM(MAC)
    [self updateContentSize];
#endif
}

#if !PLATFORM(MAC)
- (void)updateContentSize
{
    [self layoutIfNeeded];
    CGFloat height = 0;
    for (ViewType *subview in self.subviews)
        height += subview.frame.size.height;
    [self setContentSize: { self.frame.size.width, self.frame.size.height / 2 + height }];
}
#endif

- (void)layoutText
{
    [_details invalidateIntrinsicContentSize];
}

#if PLATFORM(MAC)
- (BOOL)textView:(NSTextView *)textView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex
{
    [self clickedOnLink:link];
    return YES;
}

- (void)layout
{
    [super layout];
    [self layoutText];
}
#else
- (void)layoutSubviews
{
    [super layoutSubviews];
    [self layoutText];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)textView:(UITextView *)textView shouldInteractWithURL:(NSURL *)URL inRange:(NSRange)characterRange interaction:(UITextItemInteraction)interaction
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
ALLOW_DEPRECATED_DECLARATIONS_END
{
    [self clickedOnLink:URL];
    return NO;
}

- (void)didMoveToWindow
{
    [self addContent];
}
#endif

- (void)dealloc
{
    ensureOnMainRunLoop([completionHandler = WTFMove(_completionHandler), warning = WTFMove(_warning)] () mutable {
        if (completionHandler)
            completionHandler(WebKit::ContinueUnsafeLoad::No);
    });
    [super dealloc];
}

- (void)goBackClicked
{
    if (_completionHandler)
        _completionHandler(WebKit::ContinueUnsafeLoad::No);
}

- (void)continueClicked
{
    if (_completionHandler)
        _completionHandler(WebKit::ContinueUnsafeLoad::Yes);
}

- (void)clickedOnLink:(NSURL *)link
{
    if (!_completionHandler)
        return;

    if ([link isEqual:WebKit::BrowsingWarning::visitUnsafeWebsiteSentinel()])
        return _completionHandler(WebKit::ContinueUnsafeLoad::Yes);

    if ([link isEqual:WebKit::BrowsingWarning::confirmMalwareSentinel()]) {
#if PLATFORM(MAC)
        auto alert = adoptNS([NSAlert new]);
        [alert setMessageText:WEB_UI_NSSTRING(@"Are you sure you wish to go to this site?", "Malware confirmation dialog title")];
        [alert setInformativeText:WEB_UI_NSSTRING(@"Merely visiting a site is sufficient for malware to install itself and harm your computer.", "Malware confirmation dialog")];
        [alert addButtonWithTitle:WEB_UI_NSSTRING(@"Cancel", "Cancel")];
        [alert addButtonWithTitle:WEB_UI_NSSTRING(@"Continue", "Continue")];
        [alert beginSheetModalForWindow:self.window completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<_WKWarningView>(self), alert](NSModalResponse returnCode) {
            if (auto strongSelf = weakSelf.get()) {
                if (returnCode == NSAlertSecondButtonReturn && strongSelf->_completionHandler)
                    strongSelf->_completionHandler(WebKit::ContinueUnsafeLoad::Yes);
            }
        }).get()];
#else
        _completionHandler(WebKit::ContinueUnsafeLoad::Yes);
#endif
        return;
    }

    ASSERT([link isKindOfClass:[NSURL class]]);
    _completionHandler((NSURL *)link);
}

- (BOOL)forMainFrameNavigation
{
    return _warning->forMainFrameNavigation();
}

- (RefPtr<const WebKit::BrowsingWarning>)_protectedWarning
{
    return _warning;
}

@end

@implementation _WKWarningViewTextView

- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(_WKWarningView *)warning
{
    if (!(self = [super init]))
        return nil;
    self->_warning = warning;
    self.delegate = warning;

    auto *foregroundColor = colorForItem(WarningItem::MessageText, warning);
    auto string = adoptNS([attributedString mutableCopy]);
    [string addAttributes:@{ NSForegroundColorAttributeName : foregroundColor } range:NSMakeRange(0, [string length])];
    [self setBackgroundColor:colorForItem(WarningItem::BoxBackground, warning)];
    [self setLinkTextAttributes:@{ NSForegroundColorAttributeName : foregroundColor }];
    [self.textStorage appendAttributedString:string.get()];
    self.editable = NO;
#if !PLATFORM(MAC)
    self.scrollEnabled = NO;
#endif

    return self;
}

- (SizeType)intrinsicContentSize
{
#if PLATFORM(MAC)
    [self.layoutManager ensureLayoutForTextContainer:self.textContainer];
    return { NSViewNoIntrinsicMetric, [self.layoutManager usedRectForTextContainer:self.textContainer].size.height };
#elif HAVE(SAFE_BROWSING)
    auto width = std::min<CGFloat>(maxWidth, [_warning frame].size.width) - 2 * marginSize;
    constexpr auto noHeightConstraint = CGFLOAT_MAX;
    return { width, [self sizeThatFits: { width, noHeightConstraint }].height };
#else
    return { 0, 0 };
#endif
}

#if PLATFORM(MAC)
- (BOOL)clipsToBounds
{
    return YES;
}
#endif

@end
