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
#import "SafeBrowsingResult.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/URL.h>
#import <wtf/BlockPtr.h>
#import <wtf/Language.h>

const CGFloat exclamationPointSize = 30;
const CGFloat textSize = 13;
#if PLATFORM(MAC)
const CGFloat marginSize = 20;
const size_t maxWidth = 600;
using ColorType = NSColor;
using FontType = NSFont;
using ViewType = NSView;
using BezierPathType = NSBezierPath;
using TextViewType = NSTextView;
#else
using ColorType = UIColor;
using FontType = UIFont;
using ViewType = UIView;
using BezierPathType = UIBezierPath;
using TextViewType = UITextView;
#endif

static id confirmMalwareSentinel()
{
    return @"WKConfirmMalwareSentinel";
}

static id visitUnsafeWebsiteSentinel()
{
    return @"WKVisitUnsafeWebsiteSentinel";
}

static ColorType *colorNamed(NSString *name)
{
#if PLATFORM(MAC)
#if HAVE(SAFE_BROWSING)
    return [NSColor colorNamed:name bundle:[NSBundle bundleWithIdentifier:@"com.apple.WebKit"]];
#else
    ASSERT_NOT_REACHED();
    return nil;
#endif
#else
    return [UIColor colorNamed:name inBundle:[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] compatibleWithTraitCollection:nil];
#endif
}

#if PLATFORM(MAC)
static void replace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith)
{
    [string replaceCharactersInRange:[string.string rangeOfString:toReplace] withString:replaceWith];
}

static void addLinkAndReplace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith, NSURL *linkTarget)
{
    NSMutableAttributedString *stringWithLink = [[[NSMutableAttributedString alloc] initWithString:replaceWith] autorelease];
    [stringWithLink addAttributes:@{
        NSLinkAttributeName: linkTarget,
        NSUnderlineStyleAttributeName: @1
    } range:NSMakeRange(0, replaceWith.length)];
    [string replaceCharactersInRange:[string.string rangeOfString:toReplace] withAttributedString:stringWithLink];
}
#endif

@interface WKSafeBrowsingExclamationPoint : ViewType
@end

@implementation WKSafeBrowsingExclamationPoint

- (void)drawRect:(RectType)rect
{
    [colorNamed(@"WKSafeBrowsingWarningTitle") set];
#if PLATFORM(MAC)
#define addArcWithCenter appendBezierPathWithArcWithCenter
    NSRect square = NSMakeRect(0, 0, exclamationPointSize, exclamationPointSize);
#else
    CGRect square = CGRectMake(0, 0, exclamationPointSize, exclamationPointSize);
#endif
    BezierPathType *exclamationPoint = [BezierPathType bezierPathWithOvalInRect:square];
    [exclamationPoint addArcWithCenter: { exclamationPointSize / 2, exclamationPointSize * 13 / 30 } radius:1.75 startAngle:0 endAngle:180 clockwise:YES];
    [exclamationPoint addArcWithCenter: { exclamationPointSize / 2, exclamationPointSize * 23 / 30 } radius:1.75 startAngle:180 endAngle:360 clockwise:YES];
    [exclamationPoint addArcWithCenter: { exclamationPointSize / 2, exclamationPointSize * 7 / 30 } radius:2.25 startAngle:0 endAngle:180 clockwise:YES];
    [exclamationPoint addArcWithCenter: { exclamationPointSize / 2, exclamationPointSize * 7 / 30 } radius:2.25 startAngle:180 endAngle:360 clockwise:YES];
    [exclamationPoint fill];
#if PLATFORM(MAC)
#undef addArcWithCenter
#endif
}

- (NSSize)intrinsicContentSize
{
    return { exclamationPointSize, exclamationPointSize };
}

@end

#if PLATFORM(MAC)
static NSURL *reportAnErrorURL(const WebKit::SafeBrowsingResult& result)
{
    return WebCore::URL({ }, makeString(result.reportAnErrorURLBase(), "&url=", encodeWithURLEscapeSequences(result.url()), "&hl=", defaultLanguage()));
}

static NSURL *malwareDetailsURL(const WebKit::SafeBrowsingResult& result)
{
    return WebCore::URL({ }, makeString(result.malwareDetailsURLBase(), "&site=", result.url().host(), "&hl=", defaultLanguage()));
}

static NSString *titleText(const WebKit::SafeBrowsingResult& result)
{
    if (result.isPhishing())
        return WEB_UI_NSSTRING(@"Deceptive Website Warning", "Phishing warning title");
    if (result.isMalware())
        return WEB_UI_NSSTRING(@"Malware Website Warning", "Malware warning title");
    ASSERT(result.isUnwantedSoftware());
    return WEB_UI_NSSTRING(@"Website With Harmful Software Warning", "Unwanted software warning title");
}

static NSString *warningText(const WebKit::SafeBrowsingResult& result)
{
    if (result.isPhishing())
        return WEB_UI_NSSTRING(@"This website may try to trick you into doing something dangerous, like installing software or disclosing personal or financial information, like passwords, phone numbers, or credit cards.", "Phishing warning");
    if (result.isMalware())
        return WEB_UI_NSSTRING(@"This website may attempt to install dangerous software, which could harm your computer or steal your personal or financial information, like passwords, photos, or credit cards.", "Malware warning");
    ASSERT(result.isUnwantedSoftware());
    return WEB_UI_NSSTRING(@"This website may try to trick you into installing software that harms your browsing experience, like changing your settings without your permission or showing you unwanted ads. Once installed, it may be difficult to remove.", "Unwanted software warning");
}

static NSMutableAttributedString *detailsText(const WebKit::SafeBrowsingResult& result)
{
    if (result.isPhishing()) {
        NSString *phishingDescription = WEB_UI_NSSTRING(@"Warnings are shown for websites that have been reported as deceptive. Deceptive websites try to trick you into believing they are legitimate websites you trust.", "Phishing warning description");
        NSString *learnMore = WEB_UI_NSSTRING(@"Learn more…", "Action from safe browsing warning");
        NSString *phishingActions = WEB_UI_NSSTRING(@"If you believe this website is safe, you can %report-an-error%. Or, if you understand the risks involved, you can %bypass-link%.", "Phishing warning description");
        NSString *reportAnError = WEB_UI_NSSTRING(@"report an error", "Action from safe browsing warning");
        NSString *visitUnsafeWebsite = WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning");
        
        NSMutableAttributedString *attributedString = [[[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%@ %@\n\n%@", phishingDescription, learnMore, phishingActions]] autorelease];
        addLinkAndReplace(attributedString, learnMore, learnMore, result.learnMoreURL());
        addLinkAndReplace(attributedString, @"%report-an-error%", reportAnError, reportAnErrorURL(result));
        addLinkAndReplace(attributedString, @"%bypass-link%", visitUnsafeWebsite, visitUnsafeWebsiteSentinel());
        return attributedString;
    }
    
    auto malwareOrUnwantedSoftwareDetails = [] (const WebKit::SafeBrowsingResult& result, NSString *description, NSString *statusStringToReplace, bool confirmMalware) {
        NSMutableAttributedString *malwareDescription = [[[NSMutableAttributedString alloc] initWithString:description] autorelease];
        replace(malwareDescription, @"%safeBrowsingProvider%", result.localizedProviderName());
        NSMutableAttributedString *statusLink = [[[NSMutableAttributedString alloc] initWithString:WEB_UI_NSSTRING(@"the status of “%site%”", "Part of malware description")] autorelease];
        replace(statusLink, @"%site%", result.url().host().toString());
        addLinkAndReplace(malwareDescription, statusStringToReplace, statusLink.string, malwareDetailsURL(result));
        
        NSMutableAttributedString *ifYouUnderstand = [[[NSMutableAttributedString alloc] initWithString:WEB_UI_NSSTRING(@"If you understand the risks involved, you can %visit-this-unsafe-site-link%.", "Action from safe browsing warning")] autorelease];
        addLinkAndReplace(ifYouUnderstand, @"%visit-this-unsafe-site-link%", WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning"), confirmMalware ? confirmMalwareSentinel() : visitUnsafeWebsiteSentinel());
        
        [malwareDescription appendAttributedString:[[[NSMutableAttributedString alloc] initWithString:@"\n\n"] autorelease]];
        [malwareDescription appendAttributedString:ifYouUnderstand];
        return malwareDescription;
    };
    
    if (result.isMalware())
        return malwareOrUnwantedSoftwareDetails(result, WEB_UI_NSSTRING(@"Warnings are shown for websites where malicious software has been detected. You can check the %status-link% on the %safeBrowsingProvider% diagnostic page.", "Malware warning description"), @"%status-link%", true);
    ASSERT(result.isUnwantedSoftware());
    return malwareOrUnwantedSoftwareDetails(result, WEB_UI_NSSTRING(@"Warnings are shown for websites where harmful software has been detected. You can check %the-status-of-site% on the %safeBrowsingProvider% diagnostic page.", "Unwanted software warning description"), @"%the-status-of-site%", false);
}
#endif

@interface WKSafeBrowsingTextView : TextViewType {
@package
    WeakObjCPtr<WKSafeBrowsingWarning> _target;
}
+ (instancetype)viewWithAttributedString:(NSAttributedString *)attributedString linkTarget:(WKSafeBrowsingWarning *)target;
+ (instancetype)viewWithString:(NSString *)string;
@end

@implementation WKSafeBrowsingWarning

- (instancetype)initWithFrame:(RectType)frame safeBrowsingResult:(const WebKit::SafeBrowsingResult&)result completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, WebCore::URL>&&)>&&)completionHandler
{
    if (!(self = [super initWithFrame:frame])) {
        completionHandler(WebKit::ContinueUnsafeLoad::Yes);
        return nil;
    }

    _completionHandler = WTFMove(completionHandler);
    _result = makeRef(result);

#if PLATFORM(MAC)
    self.wantsLayer = YES;
    self.layer.backgroundColor = [colorNamed(@"WKSafeBrowsingWarningBackground") CGColor];

    NSStackView *top = [NSStackView stackViewWithViews:@[
        [[WKSafeBrowsingExclamationPoint new] autorelease],
        [NSTextField labelWithAttributedString:[[[NSAttributedString alloc] initWithString:titleText(result) attributes:@{
            NSFontAttributeName:[NSFont systemFontOfSize:exclamationPointSize],
            NSForegroundColorAttributeName:colorNamed(@"WKSafeBrowsingWarningText")
        }] autorelease]]
    ]];

    WKSafeBrowsingTextView *middle = [WKSafeBrowsingTextView viewWithString:warningText(result)];

    NSStackView *bottom = [NSStackView stackViewWithViews:@[
        [NSButton buttonWithTitle:WEB_UI_NSSTRING(@"Show details", "Action from safe browsing warning") target:self action:@selector(showDetailsClicked)],
        [NSButton buttonWithTitle:WEB_UI_NSSTRING(@"Go back", "Action from safe browsing warning") target:self action:@selector(goBackClicked)]
    ]];

    for (NSStackView *view in @[top, bottom])
        [view setHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];

    StackViewType *box = [NSStackView stackViewWithViews:@[top, middle, bottom]];
    box.wantsLayer = YES;
    box.layer.backgroundColor = [[NSColor windowBackgroundColor] CGColor];
    box.layer.cornerRadius = 6;
    box.alignment = NSLayoutAttributeCenterX;
    [box.widthAnchor constraintEqualToConstant:maxWidth].active = true;
    box.edgeInsets = { marginSize, marginSize, marginSize, marginSize };
    [box setHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];
    [self addView:box inGravity:NSStackViewGravityCenter];
#else
    // FIXME: Get this working on iOS.
    completionHandler(WebKit::ContinueUnsafeLoad::Yes);
#endif
    return self;
}

#if PLATFORM(MAC)
- (void)layout
{
    for (NSView *view in self.subviews.firstObject.subviews)
        [view invalidateIntrinsicContentSize];
    [super layout];
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

- (void)clickedOnLink:(id)link
{
    if (!_completionHandler)
        return;

    if ([link isEqual:visitUnsafeWebsiteSentinel()])
        return _completionHandler(WebKit::ContinueUnsafeLoad::Yes);

    if ([link isEqual:confirmMalwareSentinel()]) {
#if PLATFORM(MAC)
        auto alert = adoptNS([NSAlert new]);
        [alert setMessageText:WEB_UI_NSSTRING(@"Are you sure you wish to go to this site?", "Malware confirmation dialog title")];
        [alert setInformativeText:WEB_UI_NSSTRING(@"Merely visiting a site is sufficient for malware to install itself and harm your computer.", "Malware confirmation dialog")];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:@"Continue"];
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

- (void)showDetailsClicked
{
#if PLATFORM(MAC)
    NSStackView *box = self.views.firstObject;
    NSStackView *bottom = box.views.lastObject;
    NSButton *showDetailsButton = bottom.views.firstObject;
    [bottom removeView:showDetailsButton];
    WKSafeBrowsingTextView *details = [WKSafeBrowsingTextView viewWithAttributedString:detailsText(*_result) linkTarget:self];
    [box addView:details inGravity:NSStackViewGravityCenter];
#else
    // FIXME: Get this working on iOS.
#endif
}

@end

@implementation WKSafeBrowsingTextView

+ (instancetype)viewWithAttributedString:(NSAttributedString *)attributedString linkTarget:(WKSafeBrowsingWarning *)target
{
    WKSafeBrowsingTextView *instance = [[self new] autorelease];
    if (!instance)
        return nil;
    instance->_target = target;

    ColorType *foregroundColor = colorNamed(@"WKSafeBrowsingWarningText");
    NSMutableAttributedString *string = [attributedString mutableCopy];
    [string addAttributes:@{
        NSForegroundColorAttributeName : foregroundColor,
        NSFontAttributeName:[FontType systemFontOfSize:textSize]
    } range:NSMakeRange(0, string.length)];

#if PLATFORM(MAC)
    [instance setLinkTextAttributes:@{ NSForegroundColorAttributeName : foregroundColor }];
    [instance setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];
    [instance setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationVertical];
    [instance.widthAnchor constraintLessThanOrEqualToConstant:maxWidth - 2 * marginSize].active = true;
    [instance setBackgroundColor:[NSColor windowBackgroundColor]];
    [instance.textStorage appendAttributedString:string];
#endif
    return instance;
}

+ (instancetype)viewWithString:(NSString *)string
{
    return [WKSafeBrowsingTextView viewWithAttributedString:[[[NSMutableAttributedString alloc] initWithString:string] autorelease] linkTarget:nil];
}

- (NSSize)intrinsicContentSize
{
    [self.layoutManager ensureLayoutForTextContainer:self.textContainer];
    auto size = [self.layoutManager usedRectForTextContainer:self.textContainer].size;
#if PLATFORM(MAC)
    return { NSViewNoIntrinsicMetric, size.height };
#else
    return { UIViewNoIntrinsicMetric, size.height };
#endif
}

- (void)clickedOnLink:(id)link atIndex:(NSUInteger)charIndex
{
    [_target clickedOnLink:link];
}

@end
