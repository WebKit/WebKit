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
    @method createWindowWithRequest:
    @abstract Create a new window and begin to load the specified request.
    @discussion The newly created window is hidden, and the window operations delegate on the
    new controllers will get a showWindow or showWindowBehindFrontmost call.
    @param request The request to load.
    @result The WebController for the WebView in the new window.
*/
- (WebController *)createWindowWithRequest:(WebResourceRequest *)request;

/*!
    @method showWindow
    @abstract Show the window that contains the top level view of the controller,
    ordering it frontmost.
    @discussion This will only be called just after createWindowWithRequest:
    is used to create a new window.
*/
- (void)showWindow;

/*!
    @method showWindowBehindFrontmost
    @abstract Show the window that contains the top level view of the controller,
    ordering it behind the main window.
    @discussion This will only be called just after createWindowWithRequest:
    is used to create a new window.
*/
- (void)showWindowBehindFrontmost;

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
    @method mouseDidMoveOverElement:modifierFlags:
    @abstract Update the window's feedback for mousing over links to reflect a new item the mouse is over
    or new modifier flags.
    @param elementInformation Dictionary that describes the element that the mouse is over, or nil.
    @param modifierFlags The modifier flags as in NSEvent.
*/
- (void)mouseDidMoveOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned int)modifierFlags;

/*!
    @method areToolbarsVisible
    @abstract Determine whether the window's toolbars are currently visible
    @discussion This method should return YES if the window has any
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
