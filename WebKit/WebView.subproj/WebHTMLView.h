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
*/
@interface WebHTMLView : NSClipView <WebDocumentView, WebDocumentDragSettings, WebDocumentSearching, WebDocumentTextEncoding>
{
@private
    WebHTMLViewPrivate *_private;
}

/*!
    @method setNeedsToApplyStyles:
    @param flag
*/
- (void)setNeedsToApplyStyles: (BOOL)flag;

/*!
    @method reapplyStyles
    @discussion Reapplies style information to the document.  This should not be called directly,
    instead call setNeedsToApplyStyles:.
*/
- (void)reapplyStyles;

/*!
    @method setContextMenusEnabled:
    @param flag
*/
- (void)setContextMenusEnabled: (BOOL)flag;

/*!
    @method contextMenusEnabled:
*/
- (BOOL)contextMenusEnabled;

/*!
    @method deselectText
*/
- (void)deselectText;

/*!
    @method selectedAttributedText
    @abstract Get an attributed string that represents the current selection.
*/
- (NSAttributedString *)selectedAttributedText;

/*!
    @method selectedText
    @abstract Get an string that represents the current selection.
*/
- (NSString *)selectedText;

@end

