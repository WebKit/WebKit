/*	
        WebStandardPanels.h
        Copyright 2002 Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebStandardPanelsPrivate;

/*!
    @class WebStandardPanels
*/
@interface WebStandardPanels : NSObject
{
@private
    WebStandardPanelsPrivate *_privatePanels;
}

/*!
    @method sharedStandardPanels
*/
+(WebStandardPanels *)sharedStandardPanels;

/*!
    @method setUseStandardAuthenticationPanel:
    @param use
*/
-(void)setUseStandardAuthenticationPanel:(BOOL)use;

/*!
    @method useStandardAuthenticationPanel
*/
-(BOOL)useStandardAuthenticationPanel;

/*!
    @method didStartLoadingURL:inWindow:
    @param URL
    @param window
*/
-(void)didStartLoadingURL:(NSURL *)URL inWindow:(NSWindow *)window;

/*!
    @method didStopLoadingURL:inWindow:
    @param URL
    @param window
*/
-(void)didStopLoadingURL:(NSURL *)URL inWindow:(NSWindow *)window;

/*!
    @method frontmostWindowLoadingURL:
    @param URL
*/
-(NSWindow *)frontmostWindowLoadingURL:(NSURL *)URL;

@end
