/*	
        WebFrameLoadDelegate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebView;
@class WebDataSource;
@class NSError;
@class WebFrame;

/*!
    @category WebFrameLoadDelegate
    @discussion A WebView's WebFrameLoadDelegate tracks changes to its frames' location(s). 
*/
@interface NSObject (WebFrameLoadDelegate)

/*!
    @method webView:didStartProvisionalLoadForFrame:
    @abstract Notifies the delegate that the provisional data source on the frame has started to load.
    @param webView The WebView sending the message
    @param frame The frame for which the provisional load has started
*/
- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didReceiveServerRedirectForProvisionalLoadForFrame:
    @abstract Notifies the delegate that the provisional data source has received a server redirect.
    @param webView The WebView sending the message
    @param frame The data source for which the redirect occurred
*/
- (void)webView:(WebView *)sender didReceiveServerRedirectForProvisionalLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didFailProvisionalLoadWithError:forFrame:
    @abstract Notifies the delegate that the provisional load has failed.
    @param webView The WebView sending the message
    @param error The error that occurred
    @param frame The data source for which the error occurred
    @discussion When webView:didFailProvisionalLoadWithError:forFrame: is called the provisional data source
    has failed to load. The frame will continue to display the committed data source if there is one.
*/
- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;

/*!
    @method webView:didCommitLoadForFrame:
    @abstract Notifies the delegate that the load has changed from provisional to committed.
    @param webView The WebView sending the message
    @param frame The frame for which the load has committed
    @discussion When a load starts, it is considered
    "provisional" until at least one byte of the new page is
    received. This is done so the old page will not be lost if the new
    page fails to load completely.

    In some cases, a single load may be committed more than once. This happens
    in the case of multipart/x-mixed-replace, also known as "server push". In this case,
    a single location change leads to multiple documents that are loaded in sequence. When
    this happens, a new commit will be sent for each document.
*/
- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didReceiveTitle:forFrame:
    @abstract Notify that the page title has been determined or has changed
    @param webView The WebView sending the message
    @param title The new page title
    @param frame The frame for which the title changed
    @discussion The title may update during loading; clients should be prepared for this.
*/
- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame;

/*!
    @method webView:didReceiveIcon:forFrame:
    @abstract Notifies the delegate that a page icon image has been received
    @param webView The WebView sending the message
    @param image The icon image
    @param frame The frame for which a page icon has been received
*/
- (void)webView:(WebView *)sender didReceiveIcon:(NSImage *)image forFrame:(WebFrame *)frame;

/*!
    @method webView:didFinishLoadForFrame:
    @abstract Notifies the delegate that the load is done
    @param webView The WebView sending the message
    @param frame The frame that finished loading
    @discussion This callback will only be received when all subresources are done loading.
*/
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didFailLoadWithError:forFrame:
    @abstract Notifies the delegate that the committed load has failed
    @param webView The WebView sending the message
    @param error The error that occurred
    @param frame The frame that failed to load
    @discussion This method is called after the load has commmitted but ended in error.
*/
- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;

/*!
    @method webView:didChangeLocationWithinPageForFrame:
    @abstract Notifies the delegate that the scroll position in a frame has changed
    @param webView The WebView sending the message
    @param frame The frame that scrolled
    @discussion This is normally used for clicks on anchors within a page
    that is already displayed. You can find the new URL from the data source object.
*/
- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame;

/*!
    @method webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:
    @abstract Notifies the delegate that the frame has received a client-side redirect that may trigger soon
    @param webView The WebView sending the message.
    @param URL The URL to be redirected to
    @param seconds Seconds in which the redirect will happen
    @param date The fire date
    @param frame The frame on which the redirect will occur
    @discussion This method can be used to keep progress feedback
    going while a client redirect is pending. A client redirect might
    be cancelled before it fires - see webView:didCancelClientRedirectForFrame:.
*/
- (void)webView:(WebView *)sender willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame;

/*!
    method webView:didCancelClientRedirectForFrame:
    @abstract Notifies the delegate that a pending client redirect has been cancelled.
    @param webView The WebView sending the message.
    @param frame The frame for which the pending redirect was cancelled
    @discussion A client redirect can be cancelled if the frame
    changes locations before the timeout.
*/
- (void)webView:(WebView *)sender didCancelClientRedirectForFrame:(WebFrame *)frame;

/*!
    @method webView:willCloseFrame:
    @abstract Notifies the delegate that a frame will be closed
    @param webView The WebView sending the message
    @param frame The frame that will be closed
    @discussion This callback happens right before WebKit is done with the frame
    and the objects that it contains.
*/
- (void)webView:(WebView *)sender willCloseFrame:(WebFrame *)frame;

@end

