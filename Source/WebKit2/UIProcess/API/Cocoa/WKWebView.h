/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit2/WKFoundation.h>

#if WK_API_ENABLED

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

@class WKBackForwardList;
@class WKBackForwardListItem;
@class WKNavigation;
@class WKWebViewConfiguration;

@protocol WKNavigationDelegate;
@protocol WKUIDelegate;

/*!
 A @link WKWebView @/link displays interactive Web content.
 @helperclass @link WKWebViewConfiguration @/link
 Used to configure @link WKWebView @/link instances.
 */
#if TARGET_OS_IPHONE
WK_API_CLASS
@interface WKWebView : UIView
#else
WK_API_CLASS
@interface WKWebView : NSView
#endif

/*! @abstract A copy of the configuration with which the @link WKWebView @/link was initialized. */
@property (nonatomic, readonly) WKWebViewConfiguration *configuration;

/*! @abstract The view's navigation delegate. */
@property (nonatomic, weak) id <WKNavigationDelegate> navigationDelegate;

/*! @abstract The view's user interface delegate. */
@property (nonatomic, weak) id <WKUIDelegate> UIDelegate;

/*! @abstract The view's back-forward list. */
@property (nonatomic, readonly) WKBackForwardList *backForwardList;

/*! @abstract Returns a view initialized with the specified frame and configuration.
 @param frame The frame for the new view.
 @param configuration The configuration for the new view.
 @result An initialized view, or nil if the object could not be initialized.
 @discussion This is a designated initializer. You can use @link -initWithFrame: @/link to
 initialize an instance with the default configuration.
 The initializer copies
 @link //apple_ref/doc/methodparam/WKWebView/initWithFrame:configuration:/configuration
 configuration@/link, so mutating it after initialization has no effect on the
 @link WKWebView @/link instance.
 */
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration WK_DESIGNATED_INITIALIZER;

/*! @abstract Navigates to the given NSURLRequest.
 @param request The NSURLRequest to navigate to.
 @result A new navigation for the given request.
 */
- (WKNavigation *)loadRequest:(NSURLRequest *)request;

/*! @abstract Navigates to an item from the back-forward list and sets it as the current item.
 @param item The item to navigate to. Must be one of the items in the receiver's back-forward list.
 @result A new navigation to the requested item, or nil if it is the current item or the item is not part of the view's back-forward list.
 @seealso backForwardList
 */
- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item;

/*! @abstract The page title. @link WKWebView @/link is KVO-compliant for this property.
 */
@property (nonatomic, readonly) NSString *title;

/*! @abstract The active URL. @link WKWebView @/link is KVO-compliant for this property.
 @discussion This is the URL that should be reflected in the user interface.
 */
@property (nonatomic, readonly) NSURL *URL;

/*! @abstract Whether the view is loading content. @link WKWebView @/link is KVO-compliant for this
 property. */
@property (nonatomic, readonly, getter=isLoading) BOOL loading;

/*! @abstract An estimate of the fraction complete for a document load. @link WKWebView @/link is
 KVO-compliant for this property.
 @discussion This value will range from 0 to 1 and, once a load completes, will remain at 1.0
 until a new load starts, at which point it will be reset to 0. The value is an estimate based
 on the total number of bytes expected to be received for a document,
 including all its possible subresources.
 */
@property (nonatomic, readonly) double estimatedProgress;

/*! @abstract Whether all of the resources on the page have been loaded over securely encrypted connections.
 @link WKWebView @/link is KVO-compliant for this property.
 */
@property (nonatomic, readonly) BOOL hasOnlySecureContent;

/*! @abstract Whether there's a back item in the back-forward list that can be navigated to.
 @seealso backForwardList.
 */
@property (nonatomic, readonly) BOOL canGoBack;

/*! @abstract Whether there's a forward item in the back-forward list that can be navigated to.
 @seealso backForwardList.
 */
@property (nonatomic, readonly) BOOL canGoForward;

/*! @abstract Navigates to the back item in the back-forward list.
 @result A new navigation to the requested item, or nil if there is no back item in the back-forward list.
 */
- (WKNavigation *)goBack;

/*! @abstract Navigates to the forward item in the back-forward list.
 @result A new navigation to the requested item, or nil if there is no forward item in the back-forward list.
 */
- (WKNavigation *)goForward;

/*! @abstract Reloads the current page.
 @result A new navigation representing the reload.
 */
- (WKNavigation *)reload;

/*! @abstract Reloads the current page, performing end-to-end revalidation using cache-validating conditionals if possible.
 @result A new navigation representing the reload.
 */
- (WKNavigation *)reloadFromOrigin;

/*! @abstract Stops loading all resources on the current page.
 */
- (void)stopLoading;

/*! @abstract Whether horizontal swipe gestures will trigger back-forward list navigations. Defaults to NO.
 */
@property (nonatomic) BOOL allowsBackForwardNavigationGestures;

#if TARGET_OS_IPHONE
/*! @abstract The scroll view associated with the web view.
 */
@property (nonatomic, readonly) UIScrollView *scrollView;
#endif

#if !TARGET_OS_IPHONE
/* @abstract Whether magnify gestures will change the WKWebView magnification. Defaults to NO.
 @discussion It is possible to set the magnification property even if allowsMagnify is set to NO.
 */
@property (nonatomic) BOOL allowsMagnification;

/* @abstract The amount by which the page content is currently scaled. Defaults to 1.0.
 */
@property (nonatomic) CGFloat magnification;

/* @abstract Magnify the page content by the given amount and center the result on the given point.
 * @param magnification The amount by which to magnify the content.
 * @param point The point (in view space) on which to center magnification.
 */
- (void)setMagnification:(CGFloat)magnification centeredAtPoint:(CGPoint)point;

#endif

@end

#if !TARGET_OS_IPHONE

@interface WKWebView (WKIBActions) <NSUserInterfaceValidations>

/*! @abstract Action method that navigates to the back item in the back-forward list.
 @param sender The object that sent this message.
 */
- (IBAction)goBack:(id)sender;

/*! @abstract Action method that navigates to the forward item in the back-forward list.
 @param sender The object that sent this message.
 */
- (IBAction)goForward:(id)sender;

/*! @abstract Action method that reloads the current page.
 @param sender The object that sent this message.
 */
- (IBAction)reload:(id)sender;

/*! @abstract Action method that reloads the current page, performing end-to-end revalidation using 
 cache-validating conditionals if possible.
 @param sender The object that sent this message.
 */
- (IBAction)reloadFromOrigin:(id)sender;

/*! @abstract Action method that stops loading all resources on the current page.
 @param sender The object that sent this message.
 */
- (IBAction)stopLoading:(id)sender;

@end

#endif

#endif
