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
#import "WKSafeBrowsingWarning.h"

#import "PageClient.h"
#import "SafeBrowsingWarning.h"
#import <WebCore/FontCocoa.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/URL.h>
#import <wtf/BlockPtr.h>
#import <wtf/Language.h>
#import <wtf/MainThread.h>

#if PLATFORM(WATCHOS)
#import "PepperUICoreSPI.h"
#endif

constexpr CGFloat exclamationPointSize = 30;
constexpr CGFloat boxCornerRadius = 6;
#if HAVE(SAFE_BROWSING)
#if PLATFORM(WATCHOS)
constexpr CGFloat marginSize = 9;
#else
constexpr CGFloat marginSize = 20;
#endif
constexpr CGFloat maxWidth = 675;
#endif

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
    Background,
    BoxBackground,
    ExclamationPoint,
    TitleText,
    MessageText,
    ShowDetailsButton,
    GoBackButton
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
#if PLATFORM(WATCHOS)
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
    ASSERT([warning isKindOfClass:[WKSafeBrowsingWarning class]]);
#if PLATFORM(MAC)

    auto colorNamed = [] (NSString *name) -> WebCore::CocoaColor * {
#if HAVE(SAFE_BROWSING)
        return [NSColor colorNamed:name bundle:[NSBundle bundleWithIdentifier:@"com.apple.WebKit"]];
#else
        ASSERT_NOT_REACHED();
        return nil;
#endif
    };

    switch (item) {
    case WarningItem::Background:
        return colorNamed(@"WKSafeBrowsingWarningBackground");
    case WarningItem::BoxBackground:
        return [NSColor windowBackgroundColor];
    case WarningItem::TitleText:
    case WarningItem::ExclamationPoint:
        return colorNamed(@"WKSafeBrowsingWarningTitle");
    case WarningItem::MessageText:
        return colorNamed(@"WKSafeBrowsingWarningText");
    case WarningItem::ShowDetailsButton:
    case WarningItem::GoBackButton:
        ASSERT_NOT_REACHED();
        return nil;
    }
#else
    UIColor *red = [UIColor colorWithRed:0.998 green:0.239 blue:0.233 alpha:1.0];
    UIColor *white = [UIColor whiteColor];
    bool narrow = warning.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;

    switch (item) {
    case WarningItem::Background:
        return red;
    case WarningItem::BoxBackground:
        return narrow ? red : white;
    case WarningItem::TitleText:
    case WarningItem::ExclamationPoint:
        return narrow ? white : red;
    case WarningItem::MessageText:
    case WarningItem::ShowDetailsButton:
        return narrow ? white : [UIColor darkTextColor];
    case WarningItem::GoBackButton:
        return narrow ? white : warning.tintColor;
    }
#endif
    ASSERT_NOT_REACHED();
    return nil;
}

@interface WKSafeBrowsingExclamationPoint : ViewType
@end

@implementation WKSafeBrowsingExclamationPoint

- (void)drawRect:(RectType)rect
{
    constexpr CGFloat centerX = exclamationPointSize / 2;
    constexpr CGFloat pointCenterY = exclamationPointSize * 7 / 30;
    constexpr CGFloat pointRadius = 2.25 * exclamationPointSize / 30;
    constexpr CGFloat lineBottomCenterY = exclamationPointSize * 13 / 30;
    constexpr CGFloat lineTopCenterY = exclamationPointSize * 23 / 30;
    constexpr CGFloat lineRadius = 1.75 * exclamationPointSize / 30;
    ViewType *warning = self.superview.superview;
#if PLATFORM(MAC)
    [colorForItem(WarningItem::ExclamationPoint, warning) set];
    NSBezierPath *exclamationPoint = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(0, 0, exclamationPointSize, exclamationPointSize)];
    [exclamationPoint appendBezierPathWithArcWithCenter: { centerX, lineBottomCenterY } radius:lineRadius startAngle:0 endAngle:180 clockwise:YES];
    [exclamationPoint appendBezierPathWithArcWithCenter: { centerX, lineTopCenterY } radius:lineRadius startAngle:180 endAngle:360 clockwise:YES];
    [exclamationPoint lineToPoint: { centerX + lineRadius, lineBottomCenterY }];
    [exclamationPoint appendBezierPathWithArcWithCenter: { centerX, pointCenterY } radius:pointRadius startAngle:0 endAngle:180 clockwise:YES];
    [exclamationPoint appendBezierPathWithArcWithCenter: { centerX, pointCenterY } radius:pointRadius startAngle:180 endAngle:360 clockwise:YES];
#else
    auto flip = [] (auto y) {
        return exclamationPointSize - y;
    };
    [colorForItem(WarningItem::BoxBackground, warning) set];
    auto square = CGRectMake(0, 0, exclamationPointSize, exclamationPointSize);
    [[UIBezierPath bezierPathWithRect:square] fill];
    
    [colorForItem(WarningItem::ExclamationPoint, warning) set];
    UIBezierPath *exclamationPoint = [UIBezierPath bezierPathWithOvalInRect:square];
    [exclamationPoint addArcWithCenter: { centerX, flip(lineTopCenterY) } radius:lineRadius startAngle:2 * piDouble endAngle:piDouble clockwise:NO];
    [exclamationPoint addArcWithCenter: { centerX, flip(lineBottomCenterY) } radius:lineRadius startAngle:piDouble endAngle:0 clockwise:NO];
    [exclamationPoint addArcWithCenter: { centerX, flip(pointCenterY) } radius:pointRadius startAngle:0 endAngle:piDouble clockwise:NO];
    [exclamationPoint addArcWithCenter: { centerX, flip(pointCenterY) } radius:pointRadius startAngle:piDouble endAngle:piDouble * 2 clockwise:NO];
    [exclamationPoint addLineToPoint: { centerX + lineRadius, flip(lineBottomCenterY) }];
    [exclamationPoint addLineToPoint: { centerX + lineRadius, flip(lineTopCenterY) }];
#endif
    [exclamationPoint fill];
}

#if PLATFORM(MAC)
- (void)viewDidChangeEffectiveAppearance
{
    [self setNeedsDisplay:YES];
}
#endif

- (NSSize)intrinsicContentSize
{
    return { exclamationPointSize, exclamationPointSize };
}

@end

static ButtonType *makeButton(WarningItem item, WKSafeBrowsingWarning *warning, SEL action)
{
    NSString *title = nil;
    if (item == WarningItem::ShowDetailsButton)
        title = WEB_UI_NSSTRING(@"Show Details", "Action from safe browsing warning");
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

@implementation WKSafeBrowsingBox

- (void)setSafeBrowsingBackgroundColor:(WebCore::CocoaColor *)color
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

@interface WKSafeBrowsingTextView : TextViewType {
@package
    WeakObjCPtr<WKSafeBrowsingWarning> _warning;
}
- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(WKSafeBrowsingWarning *)warning;
@end

@implementation WKSafeBrowsingWarning

- (instancetype)initWithFrame:(RectType)frame safeBrowsingWarning:(const WebKit::SafeBrowsingWarning&)warning completionHandler:(CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    if (!(self = [super initWithFrame:frame])) {
        completionHandler(WebKit::ContinueUnsafeLoad::Yes);
        return nil;
    }
    _completionHandler = [weakSelf = WeakObjCPtr<WKSafeBrowsingWarning>(self), completionHandler = WTFMove(completionHandler)] (std::variant<WebKit::ContinueUnsafeLoad, URL>&& result) mutable {
#if PLATFORM(WATCHOS)
        if (auto strongSelf = weakSelf.get())
            [strongSelf.get()->_previousFirstResponder becomeFirstResponder];
#endif
        completionHandler(WTFMove(result));
    };
    _warning = &warning;
#if PLATFORM(MAC)
    [self setSafeBrowsingBackgroundColor:colorForItem(WarningItem::Background, self)];
    [self addContent];
#else
    [self setBackgroundColor:colorForItem(WarningItem::Background, self)];
#endif

#if PLATFORM(WATCHOS)
    self.crownInputScrollDirection = PUICCrownInputScrollDirectionVertical;
#endif
    return self;
}

- (void)addContent
{
    auto exclamationPoint = adoptNS([WKSafeBrowsingExclamationPoint new]);
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
    auto showDetails = makeButton(WarningItem::ShowDetailsButton, self, @selector(showDetailsClicked));
    auto goBack = makeButton(WarningItem::GoBackButton, self, @selector(goBackClicked));
    auto box = adoptNS([WKSafeBrowsingBox new]);
    _box = box.get();
    [box setSafeBrowsingBackgroundColor:colorForItem(WarningItem::BoxBackground, self)];
    [box layer].cornerRadius = boxCornerRadius;

    for (ViewType *view in @[exclamationPoint.get(), title.get(), warning.get(), goBack, showDetails]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [box addSubview:view];
    }
    [box setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:box.get()];

#if PLATFORM(WATCHOS)
    [NSLayoutConstraint activateConstraints:@[
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[exclamationPoint leadingAnchor]] constraintEqualToAnchor:[[exclamationPoint trailingAnchor] anchorWithOffsetToAnchor:[box trailingAnchor]]],
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[title leadingAnchor]] constraintEqualToConstant:marginSize],
        [[[title bottomAnchor] anchorWithOffsetToAnchor:[warning topAnchor]] constraintEqualToConstant:marginSize],
        [[[exclamationPoint bottomAnchor] anchorWithOffsetToAnchor:[title topAnchor]] constraintEqualToConstant:marginSize],
        [[[box topAnchor] anchorWithOffsetToAnchor:[exclamationPoint topAnchor]] constraintEqualToConstant:marginSize + self.frame.size.height / 2],
        [[self.topAnchor anchorWithOffsetToAnchor:[box topAnchor]] constraintEqualToAnchor:[[box bottomAnchor] anchorWithOffsetToAnchor:self.bottomAnchor] multiplier:0.2],
    ]];
#elif HAVE(SAFE_BROWSING)
    [NSLayoutConstraint activateConstraints:@[
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[exclamationPoint leadingAnchor]] constraintEqualToConstant:marginSize],
        [[[box leadingAnchor] anchorWithOffsetToAnchor:[title leadingAnchor]] constraintEqualToConstant:marginSize * 1.5 + exclamationPointSize],
        [[[title topAnchor] anchorWithOffsetToAnchor:[exclamationPoint topAnchor]] constraintEqualToAnchor:[[exclamationPoint bottomAnchor] anchorWithOffsetToAnchor:[title bottomAnchor]]],
        [[[title bottomAnchor] anchorWithOffsetToAnchor:[warning topAnchor]] constraintEqualToConstant:marginSize],
        [[[box topAnchor] anchorWithOffsetToAnchor:[title topAnchor]] constraintEqualToConstant:marginSize],
        [[self.topAnchor anchorWithOffsetToAnchor:[box topAnchor]] constraintEqualToAnchor:[[box bottomAnchor] anchorWithOffsetToAnchor:self.bottomAnchor] multiplier:0.5],
    ]];
#endif

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
    
    bool needsVerticalButtonLayout = buttonSize(showDetails).width + buttonSize(goBack).width + 3 * marginSize > self.frame.size.width;
    if (needsVerticalButtonLayout) {
        [NSLayoutConstraint activateConstraints:@[
            [[showDetails.trailingAnchor anchorWithOffsetToAnchor:[box trailingAnchor]] constraintEqualToConstant:marginSize],
            [[goBack.bottomAnchor anchorWithOffsetToAnchor:showDetails.topAnchor] constraintEqualToConstant:marginSize],
            [[goBack.bottomAnchor anchorWithOffsetToAnchor:[box bottomAnchor]] constraintEqualToConstant:marginSize * 2 + buttonSize(showDetails).height],
        ]];
    } else {
        [NSLayoutConstraint activateConstraints:@[
            [[showDetails.trailingAnchor anchorWithOffsetToAnchor:goBack.leadingAnchor] constraintEqualToConstant:marginSize],
            [goBack.topAnchor constraintEqualToAnchor:showDetails.topAnchor],
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
    WKSafeBrowsingBox *box = _box.get().get();
    ButtonType *showDetails = box.subviews.lastObject;
    [showDetails removeFromSuperview];

    auto text = adoptNS([_warning->details() mutableCopy]);
    [text addAttributes:@{ NSFontAttributeName:fontOfSize(WarningTextSize::Body) } range:NSMakeRange(0, [text length])];
    auto details = adoptNS([[WKSafeBrowsingTextView alloc] initWithAttributedString:text.get() forWarning:self]);
    _details = details.get();
    auto bottom = adoptNS([WKSafeBrowsingBox new]);
    [bottom setSafeBrowsingBackgroundColor:colorForItem(WarningItem::BoxBackground, self)];
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

    auto line = adoptNS([WKSafeBrowsingBox new]);
    [line setSafeBrowsingBackgroundColor:[WebCore::CocoaColor lightGrayColor]];
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

- (BOOL)textView:(UITextView *)textView shouldInteractWithURL:(NSURL *)URL inRange:(NSRange)characterRange interaction:(UITextItemInteraction)interaction
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

- (void)clickedOnLink:(NSURL *)link
{
    if (!_completionHandler)
        return;

    if ([link isEqual:WebKit::SafeBrowsingWarning::visitUnsafeWebsiteSentinel()])
        return _completionHandler(WebKit::ContinueUnsafeLoad::Yes);

    if ([link isEqual:WebKit::SafeBrowsingWarning::confirmMalwareSentinel()]) {
#if PLATFORM(MAC)
        auto alert = adoptNS([NSAlert new]);
        [alert setMessageText:WEB_UI_NSSTRING(@"Are you sure you wish to go to this site?", "Malware confirmation dialog title")];
        [alert setInformativeText:WEB_UI_NSSTRING(@"Merely visiting a site is sufficient for malware to install itself and harm your computer.", "Malware confirmation dialog")];
        [alert addButtonWithTitle:WEB_UI_NSSTRING(@"Cancel", "Cancel")];
        [alert addButtonWithTitle:WEB_UI_NSSTRING(@"Continue", "Continue")];
        [alert beginSheetModalForWindow:self.window completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKSafeBrowsingWarning>(self), alert](NSModalResponse returnCode) {
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

@end

@implementation WKSafeBrowsingTextView

- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(WKSafeBrowsingWarning *)warning
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

@end
