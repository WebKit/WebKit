/*
        WebWindowContext.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebController;

/*!
    @protocol WebWindowContext
    @discussion A class that implements WebWindowContext provides window-related methods
    that may be used by Javascript, plugins and other aspects of web pages.
*/
@protocol WebWindowContext <NSObject>
/*!
    @method openNewWindowWithURL:referrer:
    @param URL
    @param referrer
*/
- (WebController *)openNewWindowWithURL:(NSURL *)URL referrer:(NSString *)referrer;

/*!
    @method setStatusText:
    @param text
*/
- (void)setStatusText:(NSString *)text;

/*!
    @method statusText
*/
- (NSString *)statusText;

/*!
    @method areToolbarsVisible
*/
- (BOOL)areToolbarsVisible;

/*!
    @method setToolbarsVisible:
    @param visible
*/
- (void)setToolbarsVisible:(BOOL)visible;

/*!
    @method isStatusBarVisible
*/
- (BOOL)isStatusBarVisible;

/*!
    @method setStatusBarVisible:
    @param visible
*/
- (void)setStatusBarVisible:(BOOL)visible;

/*!
    @method setFrame:
    @discussion Even though a caller could set the frame directly using the NSWindow,
    this method is provided so implementors of this protocol can do special
    things on programmatic move/resize, like avoiding autosaving of the size.
    @param frame
*/
- (void)setFrame:(NSRect)frame;
   
/*!
    @method window
*/
- (NSWindow *)window;

@end


