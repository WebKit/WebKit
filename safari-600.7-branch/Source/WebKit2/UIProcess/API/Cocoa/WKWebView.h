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

#import <WebKit/WKFoundation.h>

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
 A WKWebView object displays interactive Web content.
 @helperclass @link WKWebViewConfiguration @/link
 Used to configure @link WKWebView @/link instances.
 */
#if TARGET_OS_IPHONE
WK_CLASS_AVAILABLE(10_10, 8_0)
@interface WKWebView : UIView
#else
WK_CLASS_AVAILABLE(10_10, 8_0)
@interface WKWebView : NSView
#endif

/*! @abstract A copy of the configuration with which the web view was
 initialized. */
@property (nonatomic, readonly, copy) WKWebViewConfiguration *configuration;

/*! @abstract The web view's navigation delegate. */
@property (nonatomic, weak) id <WKNavigationDelegate> navigationDelegate;

/*! @abstract The web view's user interface delegate. */
@property (nonatomic, weak) id <WKUIDelegate> UIDelegate;

/*! @abstract The web view's back-forward list. */
@property (nonatomic, readonly, strong) WKBackForwardList *backForwardList;

/*! @abstract Returns a web view initialized with a specified frame and
 configuration.
 @param frame The frame for the new web view.
 @param configuration The configuration for the new web view.
 @result An initialized web view, or nil if the object could not be
 initialized.
 @discussion This is a designated initializer. You can use
 @link -initWithFrame: @/link to initialize an instance with the default
 configuration. The initializer copies the specified configuration, so
 mutating the configuration after invoking the initializer has no effect
 on the web view.
 */
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration WK_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder *)coder WK_UNAVAILABLE;

/*! @abstract Navigates to a requested URL.
 @param request The request specifying the URL to which to navigate.
 @result A new navigation for the given request.
 */
- (WKNavigation *)loadRequest:(NSURLRequest *)request;

/*! @abstract Sets the webpage contents and base URL.
 @param string The string to use as the contents of the webpage.
 @param baseURL A URL that is used to resolve relative URLs within the document.
 @result A new navigation.
 */
- (WKNavigation *)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL;

/*! @abstract Navigates to an item from the back-forward list and sets it
 as the current item.
 @param item The item to which to navigate. Must be one of the items in the
 web view's back-forward list.
 @result A new navigation to the requested item, or nil if it is already
 the current item or is not part of the web view's back-forward list.
 @seealso backForwardList
 */
- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item;

/*! @abstract The page title.
 @discussion @link WKWebView @/link is key-value observing (KVO) compliant
 for this property.
 */
@property (nonatomic, readonly, copy) NSString *title;

/*! @abstract The active URL.
 @discussion This is the URL that should be reflected in the user
 interface.
 @link WKWebView @/link is key-value observing (KVO) compliant for this
 property.
 */
@property (nonatomic, readonly, copy) NSURL *URL;

/*! @abstract A Boolean value indicating whether the view is currently
 loading content.
 @discussion @link WKWebView @/link is key-value observing (KVO) compliant
 for this property.
 */
@property (nonatomic, readonly, getter=isLoading) BOOL loading;

/*! @abstract An estimate of what fraction of the current navigation has been completed.
 @discussion This value ranges from 0.0 to 1.0 based on the total number of
 bytes expected to be received, including the main document and all of its
 potential subresources. After a navigation completes, the value remains at 1.0
 until a new navigation starts, at which point it is reset to 0.0.
 @link WKWebView @/link is key-value observing (KVO) compliant for this
 property.
 */
@property (nonatomic, readonly) double estimatedProgress;

/*! @abstract A Boolean value indicating whether all resources on the page
 have been loaded over securely encrypted connections.
 @discussion @link WKWebView @/link is key-value observing (KVO) compliant
 for this property.
 */
@property (nonatomic, readonly) BOOL hasOnlySecureContent;

/*! @abstract A Boolean value indicating whether there is a back item in
 the back-forward list that can be navigated to.
 @discussion @link WKWebView @/link is key-value observing (KVO) compliant
 for this property.
 @seealso backForwardList.
 */
@property (nonatomic, readonly) BOOL canGoBack;

/*! @abstract A Boolean value indicating whether there is a forward item in
 the back-forward list that can be navigated to.
 @discussion @link WKWebView @/link is key-value observing (KVO) compliant
 for this property.
 @seealso backForwardList.
 */
@property (nonatomic, readonly) BOOL canGoForward;

/*! @abstract Navigates to the back item in the back-forward list.
 @result A new navigation to the requested item, or nil if there is no back
 item in the back-forward list.
 */
- (WKNavigation *)goBack;

/*! @abstract Navigates to the forward item in the back-forward list.
 @result A new navigation to the requested item, or nil if there is no
 forward item in the back-forward list.
 */
- (WKNavigation *)goForward;

/*! @abstract Reloads the current page.
 @result A new navigation representing the reload.
 */
- (WKNavigation *)reload;

/*! @abstract Reloads the current page, performing end-to-end revalidation
 using cache-validating conditionals if possible.
 @result A new navigation representing the reload.
 */
- (WKNavigation *)reloadFromOrigin;

/*! @abstract Stops loading all resources on the current page.
 */
- (void)stopLoading;

/* @abstract Evaluates the given JavaScript string.
 @param javaScriptString The JavaScript string to evaluate.
 @param completionHandler A block to invoke when script evaluation completes or fails.
 @discussion The completionHandler is passed the result of the script evaluation or an error.
*/
- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler;

/*! @abstract A Boolean value indicating whether horizontal swipe gestures
 will trigger back-forward list navigations.
 @discussion The default value is NO.
 */
@property (nonatomic) BOOL allowsBackForwardNavigationGestures;

#if TARGET_OS_IPHONE
/*! @abstract The scroll view associated with the web view.
 */
@property (nonatomic, readonly, strong) UIScrollView *scrollView;
#endif

#if !TARGET_OS_IPHONE
/* @abstract A Boolean value indicating whether magnify gestures will
 change the web view's magnification.
 @discussion It is possible to set the magnification property even if
 allowsMagnification is set to NO.
 The default value is NO.
 */
@property (nonatomic) BOOL allowsMagnification;

/* @abstract The factor by which the page content is currently scaled.
 @discussion The default value is 1.0.
 */
@property (nonatomic) CGFloat magnification;

/* @abstract Scales the page content by a specified factor and centers the
 result on a specified point.
 * @param magnification The factor by which to scale the content.
 * @param point The point (in view space) on which to center magnification.
 */
- (void)setMagnification:(CGFloat)magnification centeredAtPoint:(CGPoint)point;

#endif

@end

#if !TARGET_OS_IPHONE

@interface WKWebView (WKIBActions) <NSUserInterfaceValidations>

/*! @abstract Action method that navigates to the back item in the
 back-forward list.
 @param sender The object that sent this message.
 */
- (IBAction)goBack:(id)sender;

/*! @abstract Action method that navigates to the forward item in the
 back-forward list.
 @param sender The object that sent this message.
 */
- (IBAction)goForward:(id)sender;

/*! @abstract Action method that reloads the current page.
 @param sender The object that sent this message.
 */
- (IBAction)reload:(id)sender;

/*! @abstract Action method that reloads the current page, performing
 end-to-end revalidation using cache-validating conditionals if possible.
 @param sender The object that sent this message.
 */
- (IBAction)reloadFromOrigin:(id)sender;

/*! @abstract Action method that stops loading all resources on the current
 page.
 @param sender The object that sent this message.
 */
- (IBAction)stopLoading:(id)sender;

@end

#endif

#endif
