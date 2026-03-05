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

static RetainPtr<WebCore::CocoaFont> fontOfSize(WarningTextSize size)
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

static RetainPtr<WebCore::CocoaColor> colorForItem(WarningItem item, ViewType *warning)
{
    auto *warningView = checked_objc_cast<_WKWarningView>(warning);
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
    RetainPtr color = colorForItem(WarningItem::WarningSymbol, warningView);
    BOOL shouldSetTint = NO;
    CGFloat imagePointSize = fontOfSize(WarningTextSize::Title).get().pointSize * imageIconPointSizeMultiplier;
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
        [view setContentTintColor:color.get()];
#else
    RetainPtr view = adoptNS([[UIImageView alloc] initWithImage:[UIImage systemImageNamed:symbolName]]);
    [view setTintColor:color.get()];
    [view setPreferredSymbolConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:imagePointSize]];
    [view setContentMode:UIViewContentModeScaleAspectFit];
#endif
    return view;
}

static ButtonType *makeButton(WarningItem item, _WKWarningView *warning, SEL action)
{
    RetainPtr<NSString> title;
    if (item == WarningItem::ShowDetailsButton)
        title = WEB_UI_NSSTRING(@"Show Details", "Action from safe browsing warning");
    else if (item == WarningItem::ContinueButton)
        title = WEB_UI_NSSTRING(@"Continue", "Continue");
    else
        title = WEB_UI_NSSTRING(@"Go Back", "Action from safe browsing warning");
#if PLATFORM(MAC)
    return [NSButton buttonWithTitle:title.get() target:warning action:action];
#else
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    auto attributedTitle = adoptNS([[NSAttributedString alloc] initWithString:title.get() attributes:@{
        NSUnderlineStyleAttributeName:@(NSUnderlineStyleSingle),
        NSUnderlineColorAttributeName:[UIColor whiteColor],
        NSForegroundColorAttributeName:colorForItem(item, warning).get(),
        NSFontAttributeName:fontOfSize(WarningTextSize::Body).get()
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
    self.layer.backgroundColor = RetainPtr { [_backgroundColor CGColor] }.get();
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

- (instancetype)initWithFrame:(RectType)frame browsingWarning:(const WebKit::BrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    if (!(self = [super initWithFrame:frame])) {
        completionHandler(WebKit::ContinueUnsafeLoad::Yes);
        return nil;
    }
    _completionHandler = [weakSelf = WeakObjCPtr<_WKWarningView>(self), completionHandler = WTF::move(completionHandler)] (Variant<WebKit::ContinueUnsafeLoad, URL>&& result) mutable {
#if PLATFORM(WATCHOS)
        if (auto strongSelf = weakSelf.get())
            [strongSelf.get()->_previousFirstResponder becomeFirstResponder];
#endif
        completionHandler(WTF::move(result));
    };
    _warning = &warning;
#if PLATFORM(MAC)
    [self setWarningViewBackgroundColor:colorForItem(WarningItem::BrowsingWarningBackground, self).get()];
    [self addContent];
#else
    [self setBackgroundColor:colorForItem(WarningItem::BrowsingWarningBackground, self).get()];
#endif

#if PLATFORM(WATCHOS)
    self.crownInputScrollDirection = PUICCrownInputScrollDirectionVertical;
#endif
    return self;
}

- (void)addContent
{
    RetainPtr warningViewIcon = viewForIconImage(self);
    auto title = makeLabel(adoptNS([[NSAttributedString alloc] initWithString:_warning->title().createNSString().get() attributes:@{
        NSFontAttributeName:fontOfSize(WarningTextSize::Title).get(),
        NSForegroundColorAttributeName:colorForItem(WarningItem::TitleText, self).get()
#if PLATFORM(WATCHOS)
        , NSHyphenationFactorDocumentAttribute:@1
#endif
    }]).get());
    auto warning = makeLabel(adoptNS([[NSAttributedString alloc] initWithString:_warning->warning().createNSString().get() attributes:@{
        NSFontAttributeName:fontOfSize(WarningTextSize::Body).get(),
        NSForegroundColorAttributeName:colorForItem(WarningItem::MessageText, self).get()
#if PLATFORM(WATCHOS)
        , NSHyphenationFactorDocumentAttribute:@1
#endif
    }]).get());

    RetainPtr primaryButton = WTF::switchOn(_warning->data(), [&] (const WebKit::BrowsingWarning::SafeBrowsingWarningData&) {
        return makeButton(WarningItem::ShowDetailsButton, self, @selector(showDetailsClicked));
    }, [&] (const WebKit::BrowsingWarning::HTTPSNavigationFailureData&) {
        return makeButton(WarningItem::ContinueButton, self, @selector(continueClicked));
    });
    RetainPtr goBack = makeButton(WarningItem::GoBackButton, self, @selector(goBackClicked));
    RetainPtr box = adoptNS([_WKWarningViewBox new]);
    _box = box.get();
    [box setWarningViewBackgroundColor:colorForItem(WarningItem::BoxBackground, self).get()];
    [box layer].cornerRadius = boxCornerRadius;

    for (ViewType *view in @[ warningViewIcon.get(), title.get(), warning.get(), goBack.get(), primaryButton.get() ]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [box addSubview:view];
    }
    [box setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:box.get()];

    [NSLayoutConstraint activateConstraints:@[
#if HAVE(SAFE_BROWSING)
#if PLATFORM(WATCHOS)
        [retainPtr([retainPtr([box leadingAnchor]) anchorWithOffsetToAnchor:retainPtr([warningViewIcon leadingAnchor]).get()]) constraintEqualToAnchor:retainPtr([retainPtr([warningViewIcon trailingAnchor]) anchorWithOffsetToAnchor:retainPtr([box trailingAnchor]).get()]).get()],
        [retainPtr([retainPtr([box leadingAnchor]) anchorWithOffsetToAnchor:retainPtr([title leadingAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr([warningViewIcon bottomAnchor]) anchorWithOffsetToAnchor:retainPtr([title topAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr([box topAnchor]) anchorWithOffsetToAnchor:retainPtr([warningViewIcon topAnchor]).get()]) constraintEqualToConstant:marginSize + self.frame.size.height / 2],
#else
        [retainPtr([retainPtr([box leadingAnchor]) anchorWithOffsetToAnchor:retainPtr([warningViewIcon leadingAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr([box leadingAnchor]) anchorWithOffsetToAnchor:retainPtr([title leadingAnchor]).get()]) constraintEqualToConstant:marginSize * 1.5 + warningSymbolPointSize],
        [retainPtr([retainPtr([title topAnchor]) anchorWithOffsetToAnchor:retainPtr([warningViewIcon topAnchor]).get()]) constraintEqualToAnchor:retainPtr([retainPtr([warningViewIcon bottomAnchor]) anchorWithOffsetToAnchor:retainPtr([title bottomAnchor]).get()]).get()],
        [retainPtr([retainPtr([box topAnchor]) anchorWithOffsetToAnchor:retainPtr([title topAnchor]).get()]) constraintEqualToConstant:marginSize],
#endif
        [retainPtr([retainPtr([title bottomAnchor]) anchorWithOffsetToAnchor:retainPtr([warning topAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr(self.topAnchor) anchorWithOffsetToAnchor:retainPtr([box topAnchor]).get()]) constraintEqualToAnchor:retainPtr([retainPtr([box bottomAnchor]) anchorWithOffsetToAnchor:retainPtr(self.bottomAnchor).get()]).get() multiplier:topToBottomMarginMultiplier],
#endif // HAVE(SAFE_BROWSING)
    ]];

#if HAVE(SAFE_BROWSING)
    [NSLayoutConstraint activateConstraints:@[
        [retainPtr([retainPtr(self.leftAnchor) anchorWithOffsetToAnchor:retainPtr([box leftAnchor]).get()]) constraintEqualToAnchor:retainPtr([retainPtr([box rightAnchor]) anchorWithOffsetToAnchor:retainPtr(self.rightAnchor).get()]).get()],

        [retainPtr([box widthAnchor]) constraintLessThanOrEqualToConstant:maxWidth],
        [retainPtr([box widthAnchor]) constraintLessThanOrEqualToAnchor:retainPtr(self.widthAnchor).get()],

        [retainPtr([retainPtr([box leadingAnchor]) anchorWithOffsetToAnchor:retainPtr([warning leadingAnchor]).get()]) constraintEqualToConstant:marginSize],

        [retainPtr([retainPtr([title trailingAnchor]) anchorWithOffsetToAnchor:retainPtr([box trailingAnchor]).get()]) constraintGreaterThanOrEqualToConstant:marginSize],
        [retainPtr([retainPtr([warning trailingAnchor]) anchorWithOffsetToAnchor:retainPtr([box trailingAnchor]).get()]) constraintGreaterThanOrEqualToConstant:marginSize],
        [retainPtr([retainPtr(goBack.get().trailingAnchor) anchorWithOffsetToAnchor:retainPtr([box trailingAnchor]).get()]) constraintEqualToConstant:marginSize],

        [retainPtr([retainPtr([warning bottomAnchor]) anchorWithOffsetToAnchor:retainPtr(goBack.get().topAnchor).get()]) constraintEqualToConstant:marginSize],
    ]];

    bool needsVerticalButtonLayout = buttonSize(primaryButton.get()).width + buttonSize(goBack.get()).width + 3 * marginSize > self.frame.size.width;
    if (needsVerticalButtonLayout) {
        [NSLayoutConstraint activateConstraints:@[
            [retainPtr([retainPtr(primaryButton.get().trailingAnchor) anchorWithOffsetToAnchor:retainPtr([box trailingAnchor]).get()]) constraintEqualToConstant:marginSize],
            [retainPtr([retainPtr(goBack.get().bottomAnchor) anchorWithOffsetToAnchor:retainPtr(primaryButton.get().topAnchor).get()]) constraintEqualToConstant:marginSize],
            [retainPtr([retainPtr(goBack.get().bottomAnchor) anchorWithOffsetToAnchor:retainPtr([box bottomAnchor]).get()]) constraintEqualToConstant:marginSize * 2 + buttonSize(primaryButton.get()).height],
        ]];
    } else {
        [NSLayoutConstraint activateConstraints:@[
            [retainPtr([retainPtr(primaryButton.get().trailingAnchor) anchorWithOffsetToAnchor:retainPtr(goBack.get().leadingAnchor).get()]) constraintEqualToConstant:marginSize],
            [retainPtr(goBack.get().topAnchor) constraintEqualToAnchor:retainPtr(primaryButton.get().topAnchor).get()],
            [retainPtr([retainPtr(goBack.get().bottomAnchor) anchorWithOffsetToAnchor:retainPtr([box bottomAnchor]).get()]) constraintEqualToConstant:marginSize],
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
    RetainPtr box = _box.get().get();
    RetainPtr<ButtonType> showDetails = box.get().subviews.lastObject;
    [showDetails removeFromSuperview];

    auto text = adoptNS([protect(_warning)->details() mutableCopy]);
    [text addAttributes:@{ NSFontAttributeName:fontOfSize(WarningTextSize::Body).get() } range:NSMakeRange(0, [text length])];
    auto details = adoptNS([[_WKWarningViewTextView alloc] initWithAttributedString:text.get() forWarning:self]);
    _details = details.get();
    auto bottom = adoptNS([_WKWarningViewBox new]);
    [bottom setWarningViewBackgroundColor:colorForItem(WarningItem::BoxBackground, self).get()];
    [bottom layer].cornerRadius = boxCornerRadius;

#if HAVE(SAFE_BROWSING)
    constexpr auto maxY = kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
    constexpr auto minY = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
#if PLATFORM(MAC)
    box.get().layer.maskedCorners = maxY;
    [bottom layer].maskedCorners = minY;
#else
    box.get().layer.maskedCorners = minY;
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
        [retainPtr(box.get().widthAnchor) constraintEqualToAnchor:retainPtr([bottom widthAnchor]).get()],
        [retainPtr(box.get().bottomAnchor) constraintEqualToAnchor:retainPtr([bottom topAnchor]).get()],
        [retainPtr(box.get().leadingAnchor) constraintEqualToAnchor:retainPtr([bottom leadingAnchor]).get()],
        [retainPtr([line widthAnchor]) constraintEqualToAnchor:retainPtr([bottom widthAnchor]).get()],
        [retainPtr([line leadingAnchor]) constraintEqualToAnchor:retainPtr([bottom leadingAnchor]).get()],
        [retainPtr([line topAnchor]) constraintEqualToAnchor:retainPtr([bottom topAnchor]).get()],
        [retainPtr([line heightAnchor]) constraintEqualToConstant:1],
        [retainPtr([retainPtr([bottom topAnchor]) anchorWithOffsetToAnchor:retainPtr([details topAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr([details bottomAnchor]) anchorWithOffsetToAnchor:retainPtr([bottom bottomAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr([bottom leadingAnchor]) anchorWithOffsetToAnchor:retainPtr([details leadingAnchor]).get()]) constraintEqualToConstant:marginSize],
        [retainPtr([retainPtr([details trailingAnchor]) anchorWithOffsetToAnchor:retainPtr([bottom trailingAnchor]).get()]) constraintEqualToConstant:marginSize],
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
    [_details.get() invalidateIntrinsicContentSize];
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

- (UIAction *)textView:(UITextView *)textView primaryActionForTextItem:(UITextItem *)textItem defaultAction:(UIAction *)defaultAction
{
    if (textItem.contentType == UITextItemContentTypeLink)
        [self clickedOnLink:textItem.link];
    return nil;
}

#if !PLATFORM(WATCHOS)
- (UITextItemMenuConfiguration *)textView:(UITextView *)textView menuConfigurationForTextItem:(UITextItem *)textItem defaultMenu:(UIMenu *)defaultMenu
{
    // We implement this delegate method because the text view requests a menu to be presented
    // if a `nil` action is returned from `textView:primaryActionForTextItem:defaultAction:`.
    return nil;
}
#endif

- (void)didMoveToWindow
{
    [self addContent];
}
#endif

- (void)dealloc
{
    ensureOnMainRunLoop([completionHandler = WTF::move(_completionHandler), warning = WTF::move(_warning)] () mutable {
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

    if ([link isEqual:WebKit::BrowsingWarning::visitUnsafeWebsiteSentinel().get()])
        return _completionHandler(WebKit::ContinueUnsafeLoad::Yes);

    if ([link isEqual:WebKit::BrowsingWarning::confirmMalwareSentinel().get()]) {
#if PLATFORM(MAC)
        auto alert = adoptNS([NSAlert new]);
        [alert setMessageText:RetainPtr { WEB_UI_NSSTRING(@"Are you sure you wish to go to this site?", "Malware confirmation dialog title") }.get()];
        [alert setInformativeText:RetainPtr { WEB_UI_NSSTRING(@"Merely visiting a site is sufficient for malware to install itself and harm your computer.", "Malware confirmation dialog") }.get()];
        [alert addButtonWithTitle:RetainPtr { WEB_UI_NSSTRING(@"Cancel", "Cancel") }.get()];
        [alert addButtonWithTitle:RetainPtr { WEB_UI_NSSTRING(@"Continue", "Continue") }.get()];
        [alert beginSheetModalForWindow:retainPtr(self.window).get() completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<_WKWarningView>(self), alert](NSModalResponse returnCode) {
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
    _completionHandler(link);
}

- (BOOL)forMainFrameNavigation
{
    return _warning->forMainFrameNavigation();
}

@end

@implementation _WKWarningViewTextView

- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(_WKWarningView *)warning
{
    if (!(self = [super init]))
        return nil;
    self->_warning = warning;
    self.delegate = warning;

    RetainPtr foregroundColor = colorForItem(WarningItem::MessageText, warning);
    RetainPtr string = adoptNS([attributedString mutableCopy]);
    [string addAttributes:@{ NSForegroundColorAttributeName : foregroundColor.get() } range:NSMakeRange(0, [string length])];
    [self setBackgroundColor:colorForItem(WarningItem::BoxBackground, warning).get()];
    [self setLinkTextAttributes:@{ NSForegroundColorAttributeName : foregroundColor.get() }];
    [retainPtr(self.textStorage) appendAttributedString:string.get()];
    self.editable = NO;
#if !PLATFORM(MAC)
    self.scrollEnabled = NO;
#endif

    return self;
}

- (SizeType)intrinsicContentSize
{
#if PLATFORM(MAC)
    [retainPtr(self.layoutManager) ensureLayoutForTextContainer:retainPtr(self.textContainer).get()];
    return { NSViewNoIntrinsicMetric, [retainPtr(self.layoutManager) usedRectForTextContainer:retainPtr(self.textContainer).get()].size.height };
#elif HAVE(SAFE_BROWSING)
    auto width = std::min<CGFloat>(maxWidth, [_warning.get() frame].size.width) - 2 * marginSize;
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
