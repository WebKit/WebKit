/*
        WebWindowOperationsDelegate.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <WebFoundation/WebRequest.h>

/*!
    @protocol WebOpenPanelResultListener
    @discussion This protocol is used to call back with the results of
    the file open panel requested by runOpenPanelForFileButtonWithResultListener:
*/

@protocol WebOpenPanelResultListener <NSObject>

/*!
    @method chooseFilename:
    @abstract Call this method to return a filename from the file open panel.
    @param fileName
*/
- (void)chooseFilename:(NSString *)fileName;

/*!
    @method cancel
    @abstract Call this method to indicate that the file open panel was cancelled.
*/
- (void)cancel;

@end

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
- (WebController *)createWindowWithRequest:(WebRequest *)request;

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
    @method closeWindow
    @abstract Close the current window. 
    @discussion Clients showing multiple views in one window may
    choose to close only the one corresponding to this
    controller. Other clients may choose to ignore this method
    entirely.
*/
- (void)closeWindow;

/*!
    @method focusWindow
    @abstract Focus the current window (i.e. makeKeyAndOrderFront:).
    @discussion Clients showing multiple views in one window may want to
    also do something to focus the one corresponding to this controller.
*/
- (void)focusWindow;

/*!
    @method unfocusWindow
    @abstract Unfocus the current window.
    @discussion Clients showing multiple views in one window may want to
    also do something to unfocus the one corresponding to this controller.
*/
- (void)unfocusWindow;

/*!
    @method firstResponderInWindow
    @abstract Get the first responder for this window.
    @discussion This method should return the focused control in the
    controller's view, if any. If the view is out of the window
    hierarchy, this might return something than calling firstResponder
    on the real NSWindow would. It's OK to return either nil or the
    real first responder if some control not in the window has focus.
*/
- (NSResponder *)firstResponderInWindow;

/*!
    @method makeFirstResponderInWindow:
    @abstract Set the first responder for this window.
    @param responder The responder to make first (will always be a view)
    @discussion responder will always be a view that is in the view
    subhierarchy of the top-level web view for this controller. If the
    controller's top level view is currently out of the view
    hierarchy, it may be desirable to save the first responder
    elsewhere, or possibly ignore this call.
*/
- (void)makeFirstResponderInWindow:(NSResponder *)responder;


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
    @method isResizable
    @abstract Determine whether the window is resizable or not.
    @result YES if resizable, NO if not.
    @discussion If there are multiple views in the same window, they
    have have their own separate resize controls and this may need to
    be handled specially.
*/
- (BOOL)isResizable;

/*!
    @method setResizable:
    @abstract Set the window to resizable or not
    @param resizable YES if the window should be made resizable, NO if not.
    @discussion If there are multiple views in the same window, they
    have have their own separate resize controls and this may need to
    be handled specially.
*/
- (void)setResizable:(BOOL)resizable;

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
    @method frame
    @abstract REturn the window's frame rect
    @discussion 
*/
- (NSRect)frame;

/*!
    @method runJavaScriptAlertPanelWithMessage:
    @abstract Display a JavaScript alert panel
    @param message The message to display
    @discussion Clients should visually indicate that this panel comes
    from JavaScript. The panel should have a single OK button.
*/
- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message;

/*!
    @method runJavaScriptAlertPanelWithMessage:
    @abstract Display a JavaScript confirm panel
    @param message The message to display
    @result YES if the user hit OK, no if the user chose Cancel.
    @discussion Clients should visually indicate that this panel comes
    from JavaScript. The panel should have two buttons, e.g. "OK" and
    "Cancel".
*/
- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message;

/*!
    @method runJavaScriptTextInputPanelWithPrompt:defaultText:
    @abstract Display a JavaScript text input panel
    @param message The message to display
    @param defaultText The initial text for the text entry area.
    @result The typed text if the user hit OK, otherwise nil.
    @discussion Clients should visually indicate that this panel comes
    from JavaScript. The panel should have two buttons, e.g. "OK" and
    "Cancel", and an area to type text.
*/
- (NSString *)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText;

/*!
    @message runOpenPanelForFileButtonWithResultListener:
    @abstract Display a file open panel for a file input control.
    @param resultListener The object to call back with the results.
    @discussion This method is passed a callback object instead of giving a return
    value so that it can be handled with a sheet.
*/
- (void)runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener;
   
@end
