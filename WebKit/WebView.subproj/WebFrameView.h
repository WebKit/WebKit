/*	
    WebFrameView.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@class WebFrame;
@class WebFrameViewPrivate;

@protocol WebDocumentView;

/*!
    @class WebFrameView
*/
@interface WebFrameView : NSView
{
@private
    WebFrameViewPrivate *_private;
}

/*!
    @method webFrame
    @abstract Returns the WebFrame associated with this WebFrameView
    @result The WebFrameView's frame.
*/
- (WebFrame *)webFrame;

/*!
    @method documentView
    @abstract Returns the WebFrameView's document subview
    @result The subview that renders the WebFrameView's contents
*/
- (NSView <WebDocumentView> *)documentView;

/*!
    @method setAllowsScrolling:
    @abstract Sets whether the WebFrameView allows its document to be scrolled
    @param flag YES to allow the document to be scrolled, NO to disallow scrolling
*/
- (void)setAllowsScrolling:(BOOL)flag;

/*!
    @method allowsScrolling
    @abstract Returns whether the WebFrameView allows its document to be scrolled
    @result YES if the document is allowed to scroll, otherwise NO
*/
- (BOOL)allowsScrolling;

@end
