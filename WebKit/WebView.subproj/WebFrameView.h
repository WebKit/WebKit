/*	
        WebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

/*
    ============================================================================= 
*/

@class WebDataSource;
@class WebController;
@class WebViewPrivate;
@protocol WebDocumentLoading;
@protocol WebDocumentView;

@interface WebView : NSView
{
@private
    WebViewPrivate *_private;
}

- initWithFrame: (NSRect) frame;

// Note that the controller is not retained.
- (WebController *)controller;

- frameScrollView;
- (NSView <WebDocumentView> *)documentView;

- (BOOL)isDocumentHTML;

- (void)setAllowsScrolling: (BOOL)flag;
- (BOOL)allowsScrolling;

// Extends the views that WebKit supports
// The view must conform to the WebDocumentLoading protocol
+ (void)registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType;

-(void)makeDocumentViewForDataSource:(WebDataSource *)dataSource;

@end
