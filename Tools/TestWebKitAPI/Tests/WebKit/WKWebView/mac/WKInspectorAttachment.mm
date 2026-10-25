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

#if PLATFORM(MAC)

#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKInspectorPrivateForTesting.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

namespace TestWebKitAPI {

static RetainPtr<NSWindow> createWindow(NSWindowStyleMask additionalStyleMask = 0)
{
    return adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1000, 700) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | additionalStyleMask backing:NSBackingStoreBuffered defer:NO]);
}

static RetainPtr<WKWebView> createWebView(BOOL usesAutoLayout = YES)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._developerExtrasEnabled = YES;
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 700) configuration:configuration.get()]);
    [webView setTranslatesAutoresizingMaskIntoConstraints:!usesAutoLayout];
    [webView loadHTMLString:@"<body>Inspected page</body>" baseURL:nil];
    return webView;
}

static RetainPtr<NSArray<NSLayoutConstraint *>> pinToEdges(NSView *view, NSView *container)
{
    RetainPtr<NSArray<NSLayoutConstraint *>> constraints = @[
        [view.topAnchor constraintEqualToAnchor:container.topAnchor],
        [view.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
        [view.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [view.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints.get()];
    return constraints;
}

static NSRect frameInWindow(NSView *view)
{
    return [view convertRect:view.bounds toView:nil];
}

static void showInspector(WKWebView *webView)
{
    [[webView _inspector] show];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView _inspector] isVisible];
    }));
}

static void attachInspector(WKWebView *webView)
{
    showInspector(webView);
    [[webView _inspector] attach];
    [webView.window layoutIfNeeded];
}

static void expectInspectorAttachedNextTo(WKWebView *webView)
{
    RetainPtr inspectorWebView = [[webView _inspector] inspectorWebView];
    EXPECT_EQ([inspectorWebView window], webView.window);

    NSRect inspectedFrame = frameInWindow(webView);
    NSRect inspectorFrame = frameInWindow(inspectorWebView.get());
    EXPECT_GT(NSHeight(inspectedFrame), 0);
    EXPECT_GT(NSHeight(inspectorFrame), 0);
    EXPECT_TRUE(NSIsEmptyRect(NSIntersectionRect(inspectedFrame, inspectorFrame)));
}

// Detaching first keeps the inspector from opening attached in later tests.
static void detachAndCloseInspector(WKWebView *webView)
{
    [[webView _inspector] detach];
    [webView.window layoutIfNeeded];
    EXPECT_NE([[[webView _inspector] inspectorWebView] window], webView.window);
    [[webView _inspector] close];
}

// Attaching makes the inspector open attached from then on.
static void resetInspectorToOpenDetached()
{
    RetainPtr window = createWindow();
    RetainPtr webView = createWebView(NO);
    [[window contentView] addSubview:webView.get()];
    showInspector(webView.get());
    detachAndCloseInspector(webView.get());
}

static void expectActive(NSArray<NSLayoutConstraint *> *constraints)
{
    for (NSLayoutConstraint *constraint in constraints)
        EXPECT_TRUE(constraint.active);
}

TEST(WKInspectorAttachment, WebViewUsingAutoresizing)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView(NO);
    [webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [contentView addSubview:webView.get()];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ([webView superview], contentView.get());

    detachAndCloseInspector(webView.get());
    EXPECT_TRUE(NSEqualRects([webView frame], [contentView bounds]));
}

TEST(WKInspectorAttachment, WebViewUsingAutoLayout)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    RetainPtr constraints = pinToEdges(webView.get(), contentView.get());
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_LT(NSHeight([webView frame]), NSHeight([contentView bounds]));

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_EQ([webView superview], contentView.get());
    EXPECT_EQ([[contentView subviews] count], 1U);
    EXPECT_FALSE([webView translatesAutoresizingMaskIntoConstraints]);
    expectActive(constraints.get());
    EXPECT_TRUE(NSEqualRects([webView frame], [contentView bounds]));

    // The app's constraints keep working.
    [constraints.get()[0] setConstant:50];
    [window layoutIfNeeded];
    EXPECT_EQ(NSHeight([webView frame]), NSHeight([contentView bounds]) - 50);
}

TEST(WKInspectorAttachment, AppReplacesConstraintWhileAttached)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    RetainPtr constraints = pinToEdges(webView.get(), contentView.get());
    RetainPtr<NSLayoutConstraint> originalTop = constraints.get()[0];
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());

    [NSLayoutConstraint deactivateConstraints:@[ originalTop.get() ]];
    RetainPtr replacementTop = [[webView topAnchor] constraintEqualToAnchor:[contentView topAnchor] constant:50];
    [replacementTop setActive:YES];
    [window layoutIfNeeded];

    // While the inspector is attached, the container keeps the layout that the app had when it attached, so the replacement
    // conflicts with it, and only takes effect after detaching.
    EXPECT_FALSE([originalTop isActive]);
    EXPECT_TRUE([replacementTop isActive]);
    EXPECT_NE([webView superview], contentView.get());

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_FALSE([originalTop isActive]);
    EXPECT_TRUE([replacementTop isActive]);
    EXPECT_EQ(NSHeight([webView frame]), NSHeight([contentView bounds]) - 50);
}

TEST(WKInspectorAttachment, CloseWhileAttachedToWebViewUsingAutoLayout)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    RetainPtr constraints = pinToEdges(webView.get(), contentView.get());
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());

    [[webView _inspector] close];
    [window layoutIfNeeded];
    EXPECT_EQ([webView superview], contentView.get());
    EXPECT_EQ([[contentView subviews] count], 1U);
    expectActive(constraints.get());
    EXPECT_TRUE(NSEqualRects([webView frame], [contentView bounds]));

    // The inspector now opens attached.
    showInspector(webView.get());
    [window layoutIfNeeded];
    expectInspectorAttachedNextTo(webView.get());
    detachAndCloseInspector(webView.get());
    expectActive(constraints.get());
}

TEST(WKInspectorAttachment, WebViewUsingAutoLayoutKeepsConstraintPriorities)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    pinToEdges(webView.get(), contentView.get());
    RetainPtr preferredWidth = [[webView widthAnchor] constraintEqualToConstant:640];
    [preferredWidth setPriority:NSLayoutPriorityDefaultLow];
    [preferredWidth setActive:YES];
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ(NSWidth([contentView bounds]), 1000);
    EXPECT_EQ(NSWidth([webView frame]), 1000);

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_TRUE([preferredWidth isActive]);
    EXPECT_EQ([preferredWidth priority], NSLayoutPriorityDefaultLow);
    EXPECT_EQ(NSWidth([contentView bounds]), 1000);
}

TEST(WKInspectorAttachment, WebViewUsingAutoLayoutKeepsSiblingOrder)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    pinToEdges(webView.get(), contentView.get());
    RetainPtr banner = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(350, 650, 300, 40)]);
    [contentView addSubview:banner.get()];
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ([[contentView subviews] lastObject], banner.get());

    detachAndCloseInspector(webView.get());
    EXPECT_EQ([[contentView subviews] firstObject], webView.get());
    EXPECT_EQ([[contentView subviews] lastObject], banner.get());
}

TEST(WKInspectorAttachment, WebViewUsingContentLayoutGuide)
{
    RetainPtr window = createWindow(NSWindowStyleMaskFullSizeContentView);
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    RetainPtr contentLayoutGuide = (NSLayoutGuide *)[window contentLayoutGuide];
    RetainPtr top = [[webView topAnchor] constraintEqualToAnchor:[contentLayoutGuide topAnchor]];
    [NSLayoutConstraint activateConstraints:@[
        top.get(),
        [[webView bottomAnchor] constraintEqualToAnchor:[contentView bottomAnchor]],
        [[webView leadingAnchor] constraintEqualToAnchor:[contentView leadingAnchor]],
        [[webView trailingAnchor] constraintEqualToAnchor:[contentView trailingAnchor]],
    ]];
    [window layoutIfNeeded];
    NSRect originalFrame = frameInWindow(webView.get());
    EXPECT_LT(NSMaxY(originalFrame), NSHeight([contentView bounds]));

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ(NSMaxY(frameInWindow(webView.get())), NSMaxY(originalFrame));

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_TRUE([top isActive]);
    EXPECT_TRUE(NSEqualRects(frameInWindow(webView.get()), originalFrame));
}

TEST(WKInspectorAttachment, WebViewInStackView)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    RetainPtr stackView = [NSStackView stackViewWithViews:@[ [NSTextField labelWithString:@"Header"], webView.get(), [NSTextField labelWithString:@"Footer"] ]];
    [stackView setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [stackView setCustomSpacing:30 afterView:webView.get()];
    [contentView addSubview:stackView.get()];
    pinToEdges(stackView.get(), contentView.get());
    [[[webView widthAnchor] constraintEqualToAnchor:[stackView widthAnchor]] setActive:YES];
    [window layoutIfNeeded];
    NSRect originalFrame = [webView frame];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ([stackView arrangedSubviews].count, 3U);
    EXPECT_EQ([stackView arrangedSubviews][1], [webView superview]);
    EXPECT_TRUE(NSEqualRects([[webView superview] frame], originalFrame));

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_EQ([stackView arrangedSubviews].count, 3U);
    EXPECT_EQ([stackView arrangedSubviews][1], webView.get());
    EXPECT_EQ([stackView customSpacingAfterView:webView.get()], 30);
    EXPECT_TRUE(NSEqualRects([webView frame], originalFrame));
}

TEST(WKInspectorAttachment, WebViewInStackViewGravityArea)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    RetainPtr stackView = adoptNS([[NSStackView alloc] init]);
    [stackView setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [contentView addSubview:stackView.get()];
    pinToEdges(stackView.get(), contentView.get());
    [stackView addView:[NSTextField labelWithString:@"Header"] inGravity:NSStackViewGravityTop];
    [stackView addView:webView.get() inGravity:NSStackViewGravityBottom];
    [NSLayoutConstraint activateConstraints:@[
        [[webView widthAnchor] constraintEqualToAnchor:[stackView widthAnchor]],
        [[webView heightAnchor] constraintEqualToConstant:500],
    ]];
    [window layoutIfNeeded];
    NSRect originalFrame = [webView frame];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ([[stackView viewsInGravity:NSStackViewGravityBottom] firstObject], [webView superview]);
    EXPECT_TRUE(NSEqualRects([[webView superview] frame], originalFrame));

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_EQ([[stackView viewsInGravity:NSStackViewGravityBottom] firstObject], webView.get());
    EXPECT_TRUE(NSEqualRects([webView frame], originalFrame));
}

TEST(WKInspectorAttachment, WebViewInSplitView)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    RetainPtr splitView = adoptNS([[NSSplitView alloc] initWithFrame:[contentView bounds]]);
    [splitView setVertical:YES];
    [splitView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [contentView addSubview:splitView.get()];
    pinToEdges(splitView.get(), contentView.get());
    RetainPtr sidebar = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 200, 700)]);
    [splitView addArrangedSubview:sidebar.get()];
    [splitView addArrangedSubview:webView.get()];
    RetainPtr minimumWidth = [[webView widthAnchor] constraintGreaterThanOrEqualToAnchor:[sidebar widthAnchor]];
    [minimumWidth setActive:YES];
    [splitView setHoldingPriority:NSLayoutPriorityDefaultLow + 10 forSubviewAtIndex:0];
    [window layoutIfNeeded];
    [splitView setPosition:200 ofDividerAtIndex:0];
    [window layoutIfNeeded];

    // A split view pane isn't moved into a container, because taking it out of the split view would remove it from the window.
    attachInspector(webView.get());
    EXPECT_EQ([webView superview], splitView.get());
    EXPECT_TRUE([minimumWidth isActive]);

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_EQ([[splitView arrangedSubviews] indexOfObject:webView.get()], 1U);
    EXPECT_FALSE([webView translatesAutoresizingMaskIntoConstraints]);
    EXPECT_TRUE([minimumWidth isActive]);
}

TEST(WKInspectorAttachment, WebViewMovedWhileAttached)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    pinToEdges(webView.get(), contentView.get());
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());

    RetainPtr newSuperview = adoptNS([[NSView alloc] init]);
    [newSuperview setTranslatesAutoresizingMaskIntoConstraints:NO];
    [contentView addSubview:newSuperview.get()];
    pinToEdges(newSuperview.get(), contentView.get());
    [webView removeFromSuperview];
    [newSuperview addSubview:webView.get()];
    pinToEdges(webView.get(), newSuperview.get());

    detachAndCloseInspector(webView.get());
    EXPECT_EQ([webView superview], newSuperview.get());
    EXPECT_FALSE([webView translatesAutoresizingMaskIntoConstraints]);
    EXPECT_EQ([[contentView subviews] count], 1U);
    [window layoutIfNeeded];
    EXPECT_FALSE([webView hasAmbiguousLayout]);
    EXPECT_TRUE(NSEqualRects([webView frame], [newSuperview bounds]));
}

TEST(WKInspectorAttachment, WebViewRemovedWhileAttached)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    auto pinToTop = [&] {
        RetainPtr<NSArray<NSLayoutConstraint *>> constraints = @[
            [[webView topAnchor] constraintEqualToAnchor:[contentView topAnchor]],
            [[webView leadingAnchor] constraintEqualToAnchor:[contentView leadingAnchor]],
            [[webView trailingAnchor] constraintEqualToAnchor:[contentView trailingAnchor]],
        ];
        [NSLayoutConstraint activateConstraints:constraints.get()];
        return constraints;
    };
    pinToTop();
    RetainPtr height = [[webView heightAnchor] constraintEqualToConstant:600];
    [height setActive:YES];
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());

    // A tab switch, for example.
    [webView removeFromSuperview];
    detachAndCloseInspector(webView.get());
    EXPECT_FALSE([webView translatesAutoresizingMaskIntoConstraints]);
    EXPECT_TRUE([height isActive]);
    EXPECT_EQ([[contentView subviews] count], 0U);

    [contentView addSubview:webView.get()];
    pinToTop();
    [window layoutIfNeeded];
    EXPECT_FALSE([webView hasAmbiguousLayout]);
    EXPECT_TRUE(NSEqualRects([webView frame], NSMakeRect(0, NSHeight([contentView bounds]) - 600, NSWidth([contentView bounds]), 600)));
}

TEST(WKInspectorAttachment, ViewInWebViewConstraintReleasedWhileAttached)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    RetainPtr<NSArray<NSLayoutConstraint *>> remainingConstraints;
    WeakObjCPtr<NSView> weakBanner;
    @autoreleasepool {
        RetainPtr banner = adoptNS([[NSView alloc] init]);
        [banner setTranslatesAutoresizingMaskIntoConstraints:NO];
        [contentView addSubview:banner.get()];
        weakBanner = banner.get();
        remainingConstraints = @[
            [[webView bottomAnchor] constraintEqualToAnchor:[contentView bottomAnchor]],
            [[webView leadingAnchor] constraintEqualToAnchor:[contentView leadingAnchor]],
            [[webView trailingAnchor] constraintEqualToAnchor:[contentView trailingAnchor]],
        ];
        [NSLayoutConstraint activateConstraints:remainingConstraints.get()];
        [NSLayoutConstraint activateConstraints:@[
            [[banner topAnchor] constraintEqualToAnchor:[contentView topAnchor]],
            [[banner leadingAnchor] constraintEqualToAnchor:[contentView leadingAnchor]],
            [[banner trailingAnchor] constraintEqualToAnchor:[contentView trailingAnchor]],
            [[banner heightAnchor] constraintEqualToConstant:50],
            [[webView topAnchor] constraintEqualToAnchor:[banner bottomAnchor]],
        ]];
        [window layoutIfNeeded];

        attachInspector(webView.get());
        expectInspectorAttachedNextTo(webView.get());
        [banner removeFromSuperview];
    }
    EXPECT_TRUE(Util::waitFor([&] {
        return !weakBanner;
    }));

    detachAndCloseInspector(webView.get());
    EXPECT_EQ([webView superview], contentView.get());
    expectActive(remainingConstraints.get());
}

TEST(WKInspectorAttachment, WebViewMovedWhileAttachedAndConstrainedAfterDetaching)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    pinToEdges(webView.get(), contentView.get());
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());

    RetainPtr newSuperview = adoptNS([[NSView alloc] init]);
    [newSuperview setTranslatesAutoresizingMaskIntoConstraints:NO];
    [contentView addSubview:newSuperview.get()];
    pinToEdges(newSuperview.get(), contentView.get());
    [webView removeFromSuperview];
    [newSuperview addSubview:webView.get()];
    Util::spinRunLoop(10);

    detachAndCloseInspector(webView.get());
    EXPECT_FALSE([webView translatesAutoresizingMaskIntoConstraints]);

    // The app adds constraints for the new place later.
    pinToEdges(webView.get(), newSuperview.get());
    [window layoutIfNeeded];
    EXPECT_FALSE([webView hasAmbiguousLayout]);
    EXPECT_TRUE(NSEqualRects([webView frame], [newSuperview bounds]));
}

TEST(WKInspectorAttachment, AppAddsViewNextToWebViewWhileAttached)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [contentView addSubview:webView.get()];
    pinToEdges(webView.get(), contentView.get());
    [window layoutIfNeeded];

    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());

    // An overlay that the app adds to the web view's superview, which is the container while the inspector is attached.
    RetainPtr overlay = adoptNS([[NSView alloc] init]);
    [overlay setTranslatesAutoresizingMaskIntoConstraints:NO];
    RetainPtr superview = [webView superview];
    [superview addSubview:overlay.get()];
    [NSLayoutConstraint activateConstraints:@[
        [[overlay topAnchor] constraintEqualToAnchor:[superview topAnchor] constant:10],
        [[overlay centerXAnchor] constraintEqualToAnchor:[webView centerXAnchor]],
        [[overlay widthAnchor] constraintEqualToConstant:300],
        [[overlay heightAnchor] constraintEqualToConstant:40],
    ]];

    detachAndCloseInspector(webView.get());
    [window layoutIfNeeded];
    EXPECT_EQ([overlay superview], contentView.get());
    EXPECT_EQ([[contentView subviews] lastObject], overlay.get());
    EXPECT_EQ([webView superview], contentView.get());
    EXPECT_FALSE([overlay hasAmbiguousLayout]);
    EXPECT_TRUE(NSEqualRects([overlay frame], NSMakeRect(350, NSHeight([contentView bounds]) - 50, 300, 40)));
}

TEST(WKInspectorAttachment, WebViewPlacedByFrameWithoutConstraints)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr webView = createWebView();
    [webView setFrame:[contentView bounds]];
    [contentView addSubview:webView.get()];

    // Nothing for a container to take over, so the inspector sets the frames as before.
    attachInspector(webView.get());
    expectInspectorAttachedNextTo(webView.get());
    EXPECT_EQ([webView superview], contentView.get());

    detachAndCloseInspector(webView.get());
    EXPECT_EQ([webView superview], contentView.get());
    EXPECT_TRUE(NSEqualRects([webView frame], [contentView bounds]));
}

// Safari attaches the inspector next to a frame-based view that contains the web view.
TEST(WKInspectorAttachment, AttachmentViewUsingAutoresizingDockedOnSides)
{
    RetainPtr window = createWindow();
    RetainPtr contentView = [window contentView];
    RetainPtr attachmentView = adoptNS([[NSView alloc] initWithFrame:[contentView bounds]]);
    [attachmentView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [contentView addSubview:attachmentView.get()];
    RetainPtr webView = createWebView(NO);
    [webView setFrame:[attachmentView bounds]];
    [webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [attachmentView addSubview:webView.get()];
    [webView _setInspectorAttachmentView:attachmentView.get()];

    auto expectInspectorBesideAttachmentView = [&](bool onRight) {
        [window layoutIfNeeded];
        RetainPtr inspectorWebView = [[webView _inspector] inspectorWebView];
        EXPECT_EQ([inspectorWebView superview], contentView.get());
        NSRect inspectorFrame = [inspectorWebView frame];
        NSRect attachmentFrame = [attachmentView frame];
        EXPECT_GT(NSWidth(inspectorFrame), 0);
        EXPECT_TRUE(NSIsEmptyRect(NSIntersectionRect(inspectorFrame, attachmentFrame)));
        EXPECT_EQ(NSWidth(inspectorFrame) + NSWidth(attachmentFrame), NSWidth([contentView bounds]));
        EXPECT_EQ(onRight ? NSMinX(inspectorFrame) : NSMinX(attachmentFrame), onRight ? NSMaxX(attachmentFrame) : NSMaxX(inspectorFrame));
    };

    showInspector(webView.get());
    [[webView _inspector] _attachRightForTesting];
    expectInspectorBesideAttachmentView(true);

    [window setContentSize:NSMakeSize(1200, 800)];
    Util::spinRunLoop(10);
    expectInspectorBesideAttachmentView(true);

    [[webView _inspector] _attachLeftForTesting];
    expectInspectorBesideAttachmentView(false);

    [window setContentSize:NSMakeSize(1000, 700)];
    Util::spinRunLoop(10);
    expectInspectorBesideAttachmentView(false);

    detachAndCloseInspector(webView.get());
    EXPECT_TRUE(NSEqualRects([attachmentView frame], [contentView bounds]));
    EXPECT_TRUE(NSEqualRects([webView frame], [attachmentView bounds]));
}

// Some superviews keep track of the inspected view as a particular child, so it stays where it is.
template<typename Callable>
static void expectStaysInSuperviewWhileAttached(WKWebView *webView, Callable&& superviewTracksWebView)
{
    RetainPtr superview = [webView superview];
    EXPECT_TRUE(superviewTracksWebView());

    attachInspector(webView);
    EXPECT_EQ([webView superview], superview.get());
    EXPECT_TRUE(superviewTracksWebView());

    detachAndCloseInspector(webView);
    EXPECT_EQ([webView superview], superview.get());
    EXPECT_TRUE(superviewTracksWebView());
}

TEST(WKInspectorAttachment, WebViewAsWindowContentView)
{
    RetainPtr window = createWindow();
    RetainPtr webView = createWebView();
    [window setContentView:webView.get()];
    // NSWindow makes its content view use autoresizing, but the app can change that.
    [webView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [window layoutIfNeeded];

    expectStaysInSuperviewWhileAttached(webView.get(), [&] {
        return [window contentView] == webView.get();
    });
}

TEST(WKInspectorAttachment, WebViewAsDocumentView)
{
    RetainPtr window = createWindow();
    RetainPtr scrollView = adoptNS([[NSScrollView alloc] initWithFrame:[[window contentView] bounds]]);
    [window setContentView:scrollView.get()];
    RetainPtr webView = createWebView();
    [scrollView setDocumentView:webView.get()];
    RetainPtr clipView = [scrollView contentView];
    [NSLayoutConstraint activateConstraints:@[
        [[webView topAnchor] constraintEqualToAnchor:[clipView topAnchor]],
        [[webView leadingAnchor] constraintEqualToAnchor:[clipView leadingAnchor]],
        [[webView widthAnchor] constraintEqualToAnchor:[clipView widthAnchor]],
        [[webView heightAnchor] constraintEqualToConstant:700],
    ]];
    [window layoutIfNeeded];

    expectStaysInSuperviewWhileAttached(webView.get(), [&] {
        return [scrollView documentView] == webView.get();
    });
}

TEST(WKInspectorAttachment, WebViewInBox)
{
    RetainPtr window = createWindow();
    RetainPtr box = adoptNS([[NSBox alloc] initWithFrame:[[window contentView] bounds]]);
    [box setBoxType:NSBoxCustom];
    [box setTitlePosition:NSNoTitle];
    [window setContentView:box.get()];
    RetainPtr webView = createWebView();
    [box setContentView:webView.get()];
    [window layoutIfNeeded];

    expectStaysInSuperviewWhileAttached(webView.get(), [&] {
        return [box contentView] == webView.get();
    });
}

TEST(WKInspectorAttachment, WebViewInTabView)
{
    RetainPtr window = createWindow();
    RetainPtr tabView = adoptNS([[NSTabView alloc] initWithFrame:[[window contentView] bounds]]);
    [tabView setTabViewType:NSNoTabsNoBorder];
    [window setContentView:tabView.get()];
    RetainPtr webView = createWebView();
    RetainPtr item = adoptNS([[NSTabViewItem alloc] initWithIdentifier:@"web"]);
    [item setView:webView.get()];
    [tabView addTabViewItem:item.get()];
    [window layoutIfNeeded];

    expectStaysInSuperviewWhileAttached(webView.get(), [&] {
        return [item view] == webView.get() && [webView superview] == tabView.get();
    });
}

TEST(WKInspectorAttachment, ClosingWindowReleasesWebViewUsingAutoLayout)
{
    WeakObjCPtr<WKWebView> weakWebView;
    @autoreleasepool {
        RetainPtr window = createWindow();
        [window setReleasedWhenClosed:NO];
        RetainPtr contentView = [window contentView];
        RetainPtr webView = createWebView();
        [contentView addSubview:webView.get()];
        pinToEdges(webView.get(), contentView.get());
        [window layoutIfNeeded];

        attachInspector(webView.get());
        expectInspectorAttachedNextTo(webView.get());
        weakWebView = webView.get();
        [window close];
    }
    EXPECT_TRUE(Util::waitFor([&] {
        return !weakWebView;
    }));

    resetInspectorToOpenDetached();
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
