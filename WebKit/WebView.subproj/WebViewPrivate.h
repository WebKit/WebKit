/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFWebController.h>

@class IFError;
@class IFLoadProgress;
@class IFWebFrame;

@interface IFWebControllerPrivate : NSObject
{
    IFWebFrame *mainFrame;
    id<IFResourceProgressHandler> resourceProgressHandler;
    id<IFWebControllerPolicyHandler> policyHandler;
}
@end

@interface IFWebController (IFPrivate);
- (void)_receivedProgress: (IFLoadProgress *)progress forResourceHandle: (IFURLHandle *)resourceHandle fromDataSource: (IFWebDataSource *)dataSource;
- (void)_receivedError: (IFError *)error forResourceHandle: (IFURLHandle *)resourceHandle partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;
- (void)_mainReceivedProgress: (IFLoadProgress *)progress forResourceHandle: (IFURLHandle *)resourceHandle fromDataSource: (IFWebDataSource *)dataSource;
- (void)_mainReceivedError: (IFError *)error forResourceHandle: (IFURLHandle *)resourceHandle partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;
- (void)_didStartLoading: (NSURL *)url;
- (void)_didStopLoading: (NSURL *)url;
@end
