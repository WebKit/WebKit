/*
        WebWindowOperationsDelegate.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebController;

/*!
    @protocol WebWindowOperationsDelegate
    @discussion A class that implements WebWindowOperationsDelegate provides
    window-related methods that may be used by Javascript, plugins and
    other aspects of web pages. These methods are used to open new
    windows and control aspects of existing windows.
*/
@protocol WebWindowOperationsDelegate <NSObject>
/*!
    @method openNewWindowWithURL:referrer:
    @abstract Open a new window and load the specified URL.
    @param URL The URL to load.
    @param referrer The referrer to use when loading the URL.
    @result The WebController for the WebView in the new window.
*/
- (WebController *)openNewWindowWithURL:(NSURL *)URL referrer:(NSString *)referrer;

/*!
    @method setStatusText:
    @abstract Set the window's status display, if any, to the specified string.
    @param text The status text to set
*/
- (void)setStatusText:(NSString *)text;

/*!
    @method statusText
    @abstract Get the currently displayed status text.
    @result The status text
*/
- (NSString *)statusText;

/*!
    @method areToolbarsVisible
    @abstract Determine whether the window's toolbars are currently visible
    @discussion This method should return true if the window has any
    toolbars that are currently on, besides the status bar. If the app
    has more than one toolbar per window, for example a regular
    command toolbar and a favorites bar, it should return YES from
    this method if at least one is on.
    @result YES if at least one toolbar is visible, otherwise NO.
*/
- (BOOL)areToolbarsVisible;

/*!
    @method setToolbarsVisible:
    @abstract Set whether the window's toolbars are currently visible.
    @param visible New value for toolbar visibility
    @discussion Setting this to YES should turn on all toolbars
    (except for a possible status bar). Setting it to NO should turn
    off all toolbars (with the same exception).
*/
- (void)setToolbarsVisible:(BOOL)visible;

/*!
    @method isStatusBarVisible
    @abstract Determine whether the status bar is visible.
    @result YES if the status bar is visible, otherwise NO.
*/
- (BOOL)isStatusBarVisible;

/*!
    @method setStatusBarVisible:
    @abstract Set whether the status bar is currently visible.
    @param visible The new visibility value
    @discussion Setting this to YES should show the status bar,
    setting it to NO should hide it.
*/
- (void)setStatusBarVisible:(BOOL)visible;

/*!
    @method setFrame:
    @abstract Set the window's frame rect
    @param frame The new window frame size
    @discussion Even though a caller could set the frame directly using the NSWindow,
    this method is provided so implementors of this protocol can do special
    things on programmatic move/resize, like avoiding autosaving of the size.
*/
- (void)setFrame:(NSRect)frame;
   
/*!
    @method window
    @abstract Get the NSWindow that contains the top-level view of the WebController
    @result The NSWindow corresponding to this WebController
*/
- (NSWindow *)window;

@end


