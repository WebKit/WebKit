/*	
    IFBaseWebController.h
    
    Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class IFWebDataSource;
@class IFWebView;
@protocol IFWebController;

@class IFBaseWebControllerPrivate;

@interface IFBaseWebController : NSObject <IFWebController>
{
@private
    IFBaseWebControllerPrivate *_private;
}

// Calls designated initializer with nil arguments.
- init;

// Designated initializer.
- initWithView: (IFWebView *)view provisionalDataSource: (IFWebDataSource *)dataSource;

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag;
- (BOOL)directsAllLinksToSystemBrowser;

@end
