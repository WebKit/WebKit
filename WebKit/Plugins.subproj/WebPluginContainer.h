/*
        WebPluginContainer.h
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

/*!
    @protocol WebPluginContainer
    @discussion This protocol enables a plug-in to request that its application
    perform certain operations.
*/

@protocol WebPluginContainer <NSObject>

/*!
    @method showURL:inFrame:
    @abstract Tell the application to show a URL in a target frame
    @param URL The URL to be shown. May not be nil.
    @param target The target frame. If the frame with the specified target is not
    found, a new window is opened and the main frame of the new window is named
    with the specified target.
*/
- (void)showURL:(NSURL *)URL inFrame:(NSString *)target;


/*!
    @method showStatus:
    @abstract Tell the application to show the specified status message.
    @param message The string to be shown. May not be nil.
*/
- (void)showStatus:(NSString *)message;
        
@end