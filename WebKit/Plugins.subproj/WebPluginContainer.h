/*
    WebPluginContainer.h
    Copyright 2004, Apple, Inc. All rights reserved.
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>

/*!
    This informal protocol enables a plug-in to request that its containing application
    perform certain operations.
*/

@interface NSObject (WebPlugInContainer)

/*!
    @method webPlugInContainerLoadRequest:inFrame:
    @abstract Tell the application to show a URL in a target frame
    @param request The request to be loaded.
    @param target The target frame. If the frame with the specified target is not
    found, a new window is opened and the main frame of the new window is named
    with the specified target.  If nil is specified, the frame that contains
    the applet is targeted.
*/
- (void)webPlugInContainerLoadRequest:(NSURLRequest *)request inFrame:(NSString *)target;

/*!
    @method webPlugInContainerShowStatus:
    @abstract Tell the application to show the specified status message.
    @param message The string to be shown.
*/
- (void)webPlugInContainerShowStatus:(NSString *)message;

/*!
	@method webPlugInContainerSelectionColor
	@result Returns the color that should be used for any special drawing when
	plug-in is selected.
*/
- (NSColor *)webPlugInContainerSelectionColor;

/*!
    @method webFrame
    @discussion The webFrame method allows the plug-in to access the WebFrame that
    contains the plug-in.  This method will not be implemented by containers that 
    are not WebKit based.
    @result Return the WebFrame that contains the plug-in.
*/
- (WebFrame *)webFrame;

@end
