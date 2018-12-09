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
#import <WebCore/LocalizedStrings.h>
#import <wtf/URL.h>
#import <wtf/BlockPtr.h>
#import <wtf/Language.h>

constexpr CGFloat exclamationPointSize = 30;
constexpr CGFloat boxCornerRadius = 6;
#if HAVE(SAFE_BROWSING)
constexpr CGFloat marginSize = 20;
constexpr CGFloat maxWidth = 675;
#endif

#if PLATFORM(MAC)
using ColorType = NSColor;
using FontType = NSFont;
using TextViewType = NSTextView;
using ButtonType = NSButton;
using AlignmentType = NSLayoutAttribute;
using ViewType = NSView;
using SizeType = NSSize;
#else
using ColorType = UIColor;
using FontType = UIFont;
using TextViewType = UITextView;
using ButtonType = UIButton;
using AlignmentType = UIStackViewAlignment;
using ViewType = UIView;
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

static FontType *fontOfSize(WarningTextSize size)
{
#if PLATFORM(MAC)
    switch (size) {
    case WarningTextSize::Title:
        return [NSFont boldSystemFontOfSize:26];
    case WarningTextSize::Body:
        return [NSFont systemFontOfSize:14];
    }
#else
    switch (size) {
    case WarningTextSize::Title:
        return [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
    case WarningTextSize::Body:
        return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    }
#endif
}

static ColorType *colorForItem(WarningItem item, ViewType *warning)
{
    ASSERT([warning isKindOfClass:[WKSafeBrowsingWarning class]]);
#if PLATFORM(MAC)

    auto colorNamed = [] (NSString *name) -> ColorType* {
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

- (NSSize)intrinsicContentSize
{
    return { exclamationPointSize, exclamationPointSize };
}

@end

static ButtonType *makeButton(WarningItem item, WKSafeBrowsingWarning *warning, SEL action)
{
    NSString *title = nil;
    if (item == WarningItem::ShowDetailsButton)
        title = WEB_UI_NSSTRING(@"Show details", "Action from safe browsing warning");
    else
        title = WEB_UI_NSSTRING(@"Go back", "Action from safe browsing warning");
    title = [title capitalizedString];
#if PLATFORM(MAC)
    return [NSButton buttonWithTitle:title target:warning action:action];
#else
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    NSAttributedString *attributedTitle = [[[NSAttributedString alloc] initWithString:title attributes:@{
        NSUnderlineStyleAttributeName:@(NSUnderlineStyleSingle),
        NSUnderlineColorAttributeName:[UIColor whiteColor],
        NSForegroundColorAttributeName:colorForItem(item, warning),
        NSFontAttributeName:fontOfSize(WarningTextSize::Body)
    }] autorelease];
    [button setAttributedTitle:attributedTitle forState:UIControlStateNormal];
    [button addTarget:warning action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
#endif
}

static ViewType *makeLabel(NSAttributedString *attributedString)
{
#if PLATFORM(MAC)
    return [NSTextField labelWithAttributedString:attributedString];
#else
    auto label = [[UILabel new] autorelease];
    label.attributedText = attributedString;
    label.lineBreakMode = NSLineBreakByWordWrapping;
    label.numberOfLines = 0;
    return label;
#endif
}

static void setBackground(ViewType *view, ColorType *color)
{
#if PLATFORM(MAC)
    view.wantsLayer = YES;
    view.layer.backgroundColor = color.CGColor;
#else
    view.backgroundColor = color;
#endif
}

@interface WKSafeBrowsingTextView : TextViewType {
@package
    WeakObjCPtr<WKSafeBrowsingWarning> _warning;
}
- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(WKSafeBrowsingWarning *)warning;
@end

@implementation WKSafeBrowsingWarning

- (instancetype)initWithFrame:(RectType)frame safeBrowsingWarning:(const WebKit::SafeBrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler
{
    if (!(self = [super initWithFrame:frame])) {
        completionHandler(WebKit::ContinueUnsafeLoad::Yes);
        return nil;
    }
    _completionHandler = WTFMove(completionHandler);
    _warning = makeRef(warning);
    setBackground(self, colorForItem(WarningItem::Background, self));
#if PLATFORM(MAC)
    [self addContent];
#endif
    return self;
}

- (void)addContent
{
    auto exclamationPoint = [[WKSafeBrowsingExclamationPoint new] autorelease];
    auto title = makeLabel([[[NSAttributedString alloc] initWithString:_warning->title() attributes:@{
        NSFontAttributeName:fontOfSize(WarningTextSize::Title),
        NSForegroundColorAttributeName:colorForItem(WarningItem::TitleText, self)
    }] autorelease]);
    auto warning = makeLabel([[[NSAttributedString alloc] initWithString:_warning->warning() attributes:@{
        NSFontAttributeName:fontOfSize(WarningTextSize::Body),
        NSForegroundColorAttributeName:colorForItem(WarningItem::MessageText, self)
    }] autorelease]);
    auto showDetails = makeButton(WarningItem::ShowDetailsButton, self, @selector(showDetailsClicked));
    auto goBack = makeButton(WarningItem::GoBackButton, self, @selector(goBackClicked));
    auto box = [[ViewType new] autorelease];
    setBackground(box, colorForItem(WarningItem::BoxBackground, self));
    box.layer.cornerRadius = boxCornerRadius;

    for (ViewType *view in @[exclamationPoint, title, warning, goBack, showDetails]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [box addSubview:view];
    }
    box.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:box];

#if HAVE(SAFE_BROWSING)
    [NSLayoutConstraint activateConstraints:@[
        [[self.topAnchor anchorWithOffsetToAnchor:box.topAnchor] constraintEqualToAnchor:[box.bottomAnchor anchorWithOffsetToAnchor:self.bottomAnchor] multiplier:0.5],
        [[self.leftAnchor anchorWithOffsetToAnchor:box.leftAnchor] constraintEqualToAnchor:[box.rightAnchor anchorWithOffsetToAnchor:self.rightAnchor]],

        [box.widthAnchor constraintLessThanOrEqualToConstant:maxWidth],
        [box.widthAnchor constraintLessThanOrEqualToAnchor:self.widthAnchor],

        [[box.leadingAnchor anchorWithOffsetToAnchor:exclamationPoint.leadingAnchor] constraintEqualToConstant:marginSize],
        [[box.leadingAnchor anchorWithOffsetToAnchor:title.leadingAnchor] constraintEqualToConstant:marginSize * 1.5 + exclamationPointSize],
        [[box.leadingAnchor anchorWithOffsetToAnchor:warning.leadingAnchor] constraintEqualToConstant:marginSize],

        [[title.trailingAnchor anchorWithOffsetToAnchor:box.trailingAnchor] constraintGreaterThanOrEqualToConstant:marginSize],
        [[warning.trailingAnchor anchorWithOffsetToAnchor:box.trailingAnchor] constraintGreaterThanOrEqualToConstant:marginSize],
        [[goBack.trailingAnchor anchorWithOffsetToAnchor:box.trailingAnchor] constraintEqualToConstant:marginSize],

        [[title.topAnchor anchorWithOffsetToAnchor:exclamationPoint.topAnchor] constraintEqualToAnchor:[exclamationPoint.bottomAnchor anchorWithOffsetToAnchor:title.bottomAnchor]],

        [goBack.topAnchor constraintEqualToAnchor:showDetails.topAnchor],
        [[showDetails.trailingAnchor anchorWithOffsetToAnchor:goBack.leadingAnchor] constraintEqualToConstant:marginSize],

        [[box.topAnchor anchorWithOffsetToAnchor:title.topAnchor] constraintEqualToConstant:marginSize],
        [[title.bottomAnchor anchorWithOffsetToAnchor:warning.topAnchor] constraintEqualToConstant:marginSize],
        [[warning.bottomAnchor anchorWithOffsetToAnchor:goBack.topAnchor] constraintEqualToConstant:marginSize],
        [[goBack.bottomAnchor anchorWithOffsetToAnchor:box.bottomAnchor] constraintEqualToConstant:marginSize]
    ]];
#endif
}

- (void)showDetailsClicked
{
    ViewType *box = self.subviews.lastObject;
    ButtonType *showDetails = box.subviews.lastObject;
    [showDetails removeFromSuperview];

    NSMutableAttributedString *text = [[_warning->details() mutableCopy] autorelease];
    [text addAttributes:@{ NSFontAttributeName:fontOfSize(WarningTextSize::Body) } range:NSMakeRange(0, text.length)];
    WKSafeBrowsingTextView *details = [[[WKSafeBrowsingTextView alloc] initWithAttributedString:text forWarning:self] autorelease];
    _details = details;
    ViewType *bottom = [[ViewType new] autorelease];
    setBackground(bottom, colorForItem(WarningItem::BoxBackground, self));
    bottom.layer.cornerRadius = boxCornerRadius;

#if HAVE(SAFE_BROWSING)
    constexpr auto maxY = kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
    constexpr auto minY = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
#if PLATFORM(MAC)
    box.layer.maskedCorners = maxY;
    bottom.layer.maskedCorners = minY;
#else
    box.layer.maskedCorners = minY;
    bottom.layer.maskedCorners = maxY;
#endif
#endif

    ViewType *line = [[ViewType new] autorelease];
    setBackground(line, [ColorType lightGrayColor]);
    for (ViewType *view in @[details, bottom, line])
        view.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:bottom];
    [bottom addSubview:line];
    [bottom addSubview:details];
#if HAVE(SAFE_BROWSING)
    [NSLayoutConstraint activateConstraints:@[
        [box.widthAnchor constraintEqualToAnchor:bottom.widthAnchor],
        [box.bottomAnchor constraintEqualToAnchor:bottom.topAnchor],
        [box.leadingAnchor constraintEqualToAnchor:bottom.leadingAnchor],
        [line.widthAnchor constraintEqualToAnchor:bottom.widthAnchor],
        [line.leadingAnchor constraintEqualToAnchor:bottom.leadingAnchor],
        [line.topAnchor constraintEqualToAnchor:bottom.topAnchor],
        [line.heightAnchor constraintEqualToConstant:1],
        [[bottom.topAnchor anchorWithOffsetToAnchor:details.topAnchor] constraintEqualToConstant:marginSize],
        [[details.bottomAnchor anchorWithOffsetToAnchor:bottom.bottomAnchor] constraintEqualToConstant:marginSize],
        [[bottom.leadingAnchor anchorWithOffsetToAnchor:details.leadingAnchor] constraintEqualToConstant:marginSize],
        [[details.trailingAnchor anchorWithOffsetToAnchor:bottom.trailingAnchor] constraintEqualToConstant:marginSize],
    ]];
#endif
    [self layoutText];
#if !PLATFORM(MAC)
    [self layoutIfNeeded];
    CGFloat height = 0;
    for (ViewType *subview in self.subviews)
        height += subview.frame.size.height;
    [self setContentSize: { self.frame.size.width, self.frame.size.height / 2 + height }];
#endif
}

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
    if (_completionHandler)
        _completionHandler(WebKit::ContinueUnsafeLoad::No);
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
        [alert beginSheetModalForWindow:self.window completionHandler:BlockPtr<void(NSModalResponse)>::fromCallable([weakSelf = WeakObjCPtr<WKSafeBrowsingWarning>(self), alert](NSModalResponse returnCode) {
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

@end

@implementation WKSafeBrowsingTextView

- (instancetype)initWithAttributedString:(NSAttributedString *)attributedString forWarning:(WKSafeBrowsingWarning *)warning
{
    if (!(self = [super init]))
        return nil;
    self->_warning = warning;
    self.delegate = warning;

    ColorType *foregroundColor = colorForItem(WarningItem::MessageText, warning);
    NSMutableAttributedString *string = [[attributedString mutableCopy] autorelease];
    [string addAttributes:@{ NSForegroundColorAttributeName : foregroundColor } range:NSMakeRange(0, string.length)];
    [self setBackgroundColor:colorForItem(WarningItem::BoxBackground, warning)];
    [self setLinkTextAttributes:@{ NSForegroundColorAttributeName : foregroundColor }];
    [self.textStorage appendAttributedString:string];
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
