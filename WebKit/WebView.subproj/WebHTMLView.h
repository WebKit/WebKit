/*	
        WebHTMLView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>

@class WebDataSource;
@class WebHTMLViewPrivate;

@protocol WebDocumentDragging;
@protocol WebDocumentElement;
@protocol WebDocumentSelection;

/*!
    @class WebHTMLView
    @discussion A document view of WebFrameView that displays HTML content.
*/
@interface WebHTMLView : NSView <WebDocumentView, WebDocumentSearching, WebDocumentText, WebDocumentDragging, WebDocumentElement, WebDocumentSelection>
{
@private
    WebHTMLViewPrivate *_private;
}

/*!
    @method setNeedsToApplyStyles:
    @abstract Sets flag to cause reapplication of style information.
    @param flag YES to apply style information, NO to not apply style information.
*/
- (void)setNeedsToApplyStyles:(BOOL)flag;

/*!
    @method reapplyStyles
    @discussion Immediately causes reapplication of style information to the view.  This should not be called directly,
    instead call setNeedsToApplyStyles:.
*/
- (void)reapplyStyles;

@end

