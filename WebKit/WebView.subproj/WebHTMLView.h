/*	
        WebHTMLView.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <WebKit/WebDocument.h>


@class WebDataSource;
@class WebController;
@class WebHTMLViewPrivate;

/*!
    @class WebHTMLView
    @discussion A document view of WebView that displays HTML content.
*/
@interface WebHTMLView : NSView <WebDocumentView, WebDocumentDragSettings, WebDocumentSearching, WebDocumentTextEncoding>
{
@private
    WebHTMLViewPrivate *_private;
}

/*!
    @method setNeedsToApplyStyles:
    @abstract Sets flag to cause reapplication of style information.
    @param flag YES to apply style information, NO to not apply style information.
*/
- (void)setNeedsToApplyStyles: (BOOL)flag;

/*!
    @method reapplyStyles
    @discussion Immediately causes reapplication of style information to the view.  This should not be called directly,
    instead call setNeedsToApplyStyles:.
*/
- (void)reapplyStyles;

/*!
    @method setContextMenusEnabled:
    @abstract Enables or disables contextual menus. 
    @param flag YES to enable contextual menus, NO to disable contextual menus.
*/
- (void)setContextMenusEnabled: (BOOL)flag;

/*!
    @method contextMenusEnabled:
    @result Returns YES if contextual menus are enabled, NO if they are not.
*/
- (BOOL)contextMenusEnabled;

/*!
    @method deselectText
    @abstract Causes a text selection to lose its selection.
*/
- (void)deselectText;

/*!
    @method selectedAttributedText
    @result Attributed string that represents the current selection.
*/
- (NSAttributedString *)selectedAttributedText;

/*!
    @method selectedText
    @result String that represents the current selection.
*/
- (NSString *)selectedText;

@end

