/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "ValidationBubble.h"

#import <UIKit/UIGeometry.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/WTFString.h>

static const CGFloat validationBubbleHorizontalPadding = 17;
static const CGFloat validationBubbleVerticalPadding = 9;
static const CGFloat validationBubbleMaxLabelWidth = 300;

@interface WebValidationBubbleViewController : UIViewController
@property (nonatomic, readonly) UILabel *label;
@property (nonatomic, readonly) CGRect labelFrame;
@end

static void invokeUIViewControllerSelector(id instance, SEL selector)
{
    objc_super superClass { instance, PAL::getUIViewControllerClass() };
    auto superclassFunction = reinterpret_cast<void(*)(objc_super*, SEL)>(objc_msgSendSuper);
    superclassFunction(&superClass, selector);
}

static void WebValidationBubbleViewController_dealloc(WebValidationBubbleViewController *instance, SEL)
{
    [instance.label release];
    [instance setValue:nil forKey:@"_label"];

    invokeUIViewControllerSelector(instance, @selector(dealloc));
}

static void WebValidationBubbleViewController_viewDidLoad(WebValidationBubbleViewController *instance, SEL)
{
    invokeUIViewControllerSelector(instance, @selector(viewDidLoad));

    UILabel *label = [PAL::allocUILabelInstance() init];
    label.font = [PAL::getUIFontClass() preferredFontForTextStyle:PAL::get_UIKit_UIFontTextStyleCallout()];
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.numberOfLines = 4;
    [instance.view addSubview:label];
    [instance setValue:label forKey:@"_label"];
}

static void WebValidationBubbleViewController_viewWillLayoutSubviews(WebValidationBubbleViewController *instance, SEL)
{
    invokeUIViewControllerSelector(instance, @selector(viewWillLayoutSubviews));

    instance.label.frame = instance.labelFrame;
}

static void WebValidationBubbleViewController_viewSafeAreaInsetsDidChange(WebValidationBubbleViewController *instance, SEL)
{
    invokeUIViewControllerSelector(instance, @selector(viewSafeAreaInsetsDidChange));

    instance.label.frame = instance.labelFrame;
}

static CGRect WebValidationBubbleViewController_labelFrame(WebValidationBubbleViewController *instance, SEL)
{
    auto frameWithPadding = UIEdgeInsetsInsetRect(instance.view.bounds, instance.view.safeAreaInsets);
    return UIEdgeInsetsInsetRect(frameWithPadding, UIEdgeInsetsMake(validationBubbleVerticalPadding, validationBubbleHorizontalPadding, validationBubbleVerticalPadding, validationBubbleHorizontalPadding));
}

static UILabel *WebValidationBubbleViewController_label(WebValidationBubbleViewController *instance, SEL)
{
    return [instance valueForKey:@"_label"];
}

static WebValidationBubbleViewController *allocWebValidationBubbleViewControllerInstance()
{
    static Class theClass = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        theClass = objc_allocateClassPair(PAL::getUIViewControllerClass(), "WebValidationBubbleViewController", 0);
        class_addMethod(theClass, @selector(dealloc), (IMP)WebValidationBubbleViewController_dealloc, "v@:");
        class_addMethod(theClass, @selector(viewDidLoad), (IMP)WebValidationBubbleViewController_viewDidLoad, "v@:");
        class_addMethod(theClass, @selector(viewWillLayoutSubviews), (IMP)WebValidationBubbleViewController_viewWillLayoutSubviews, "v@:");
        class_addMethod(theClass, @selector(viewSafeAreaInsetsDidChange), (IMP)WebValidationBubbleViewController_viewSafeAreaInsetsDidChange, "v@:");
        class_addMethod(theClass, @selector(label), (IMP)WebValidationBubbleViewController_label, "v@:");
        class_addMethod(theClass, @selector(labelFrame), (IMP)WebValidationBubbleViewController_labelFrame, "v@:");
        class_addIvar(theClass, "_label", sizeof(UILabel *), log2(sizeof(UILabel *)), "@");
        objc_registerClassPair(theClass);
    });
    return (WebValidationBubbleViewController *)[theClass alloc];
}

@interface WebValidationBubbleTapRecognizer : NSObject
@end

@implementation WebValidationBubbleTapRecognizer {
    RetainPtr<UIViewController> _popoverController;
    RetainPtr<UITapGestureRecognizer> _tapGestureRecognizer;
}

- (WebValidationBubbleTapRecognizer *)initWithPopoverController:(UIViewController *)popoverController
{
    self = [super init];
    if (!self)
        return nil;

    _popoverController = popoverController;
    _tapGestureRecognizer = adoptNS([PAL::allocUITapGestureRecognizerInstance() initWithTarget:self action:@selector(dismissPopover)]);
    [[_popoverController view] addGestureRecognizer:_tapGestureRecognizer.get()];

    return self;
}

- (void)dealloc
{
    [[_popoverController view] removeGestureRecognizer:_tapGestureRecognizer.get()];
    [super dealloc];
}

- (void)dismissPopover
{
    [_popoverController dismissViewControllerAnimated:NO completion:nil];
}

@end

@interface WebValidationBubbleDelegate : NSObject <UIPopoverPresentationControllerDelegate> {
}
@end

@implementation WebValidationBubbleDelegate

- (UIModalPresentationStyle)adaptivePresentationStyleForPresentationController:(UIPresentationController *)controller traitCollection:(UITraitCollection *)traitCollection
{
    UNUSED_PARAM(controller);
    UNUSED_PARAM(traitCollection);
    // This is needed to force UIKit to use a popover on iPhone as well.
    return UIModalPresentationNone;
}

@end

namespace WebCore {

ValidationBubble::ValidationBubble(UIView *view, const String& message, const Settings&)
    : m_view(view)
    , m_message(message)
{
    m_popoverController = adoptNS([allocWebValidationBubbleViewControllerInstance() init]);
    [m_popoverController setModalPresentationStyle:UIModalPresentationPopover];
    m_tapRecognizer = adoptNS([[WebValidationBubbleTapRecognizer alloc] initWithPopoverController:m_popoverController.get()]);

    UILabel *label = [m_popoverController label];
    label.text = message;
    m_fontSize = label.font.pointSize;
    CGSize labelSize = [label sizeThatFits:CGSizeMake(validationBubbleMaxLabelWidth, CGFLOAT_MAX)];
    [m_popoverController setPreferredContentSize:CGSizeMake(labelSize.width + validationBubbleHorizontalPadding * 2, labelSize.height + validationBubbleVerticalPadding * 2)];
}

ValidationBubble::~ValidationBubble()
{
    [m_popoverController dismissViewControllerAnimated:NO completion:nil];
}

void ValidationBubble::show()
{
    if ([m_popoverController parentViewController] || [m_popoverController presentingViewController])
        return;

    // Protect the validation bubble so it stays alive until it is effectively presented. UIKit does not deal nicely with
    // dismissing a popover that is being presented.
    RefPtr<ValidationBubble> protectedThis(this);
    [m_presentingViewController presentViewController:m_popoverController.get() animated:NO completion:[protectedThis]() {
        // Hide this popover from VoiceOver and instead announce the message.
        [protectedThis->m_popoverController view].accessibilityElementsHidden = YES;
    }];

    PAL::softLinkUIKitUIAccessibilityPostNotification(PAL::get_UIKit_UIAccessibilityAnnouncementNotification(), m_message);
}

static UIViewController *fallbackViewController(UIView *view)
{
    for (UIView *currentView = view; currentView; currentView = currentView.superview) {
        if (UIViewController *viewController = [PAL::getUIViewControllerClass() viewControllerForView:currentView])
            return viewController;
    }
    NSLog(@"Failed to find a view controller to show form validation popover");
    return nil;
}

void ValidationBubble::setAnchorRect(const IntRect& anchorRect, UIViewController *presentingViewController)
{
    if (!presentingViewController)
        presentingViewController = fallbackViewController(m_view);

    UIPopoverPresentationController *presentationController = [m_popoverController popoverPresentationController];
    m_popoverDelegate = adoptNS([[WebValidationBubbleDelegate alloc] init]);
    presentationController.delegate = m_popoverDelegate.get();
    presentationController.passthroughViews = @[ presentingViewController.view, m_view ];
    presentationController.sourceView = m_view;
    presentationController.sourceRect = CGRectMake(anchorRect.x(), anchorRect.y(), anchorRect.width(), anchorRect.height());
    m_presentingViewController = presentingViewController;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
