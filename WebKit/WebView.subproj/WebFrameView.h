/*	
        IFWebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

/*
    ============================================================================= 
*/

@class IFWebDataSource;
@class IFWebController;
@class IFWebViewPrivate;
@protocol IFDocumentLoading;

@interface IFWebView : NSView
{
@private
    IFWebViewPrivate *_private;
}

- initWithFrame: (NSRect) frame;

// Note that the controller is not retained.
- (IFWebController *)controller;

- frameScrollView;
- documentView;

- (BOOL) isDocumentHTML;

- (void)setAllowsScrolling: (BOOL)flag;
- (BOOL)allowsScrolling;

// Extends the views that WebKit supports
// The view must conform to the IFDocumentLoading protocol
+ (void)registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType;

// Called when the contentPolicy is set to IFContentPolicyShow
+ (id <IFDocumentLoading>) createViewForMIMEType:(NSString *)MIMEType;

@end
