//
//  WebBridge.h
//  WebKit
//
//  Created by Darin Adler on Thu Jun 13 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreBridge.h>

@class WebDataSource;
@class WebFrame;
@protocol WebOpenPanelResultListener;

@interface WebBridge : WebCoreBridge <WebCoreBridge>
{
    WebFrame *_frame;
    BOOL _doingClientRedirect;
    BOOL _inNextKeyViewOutsideWebFrameViews;
}

- (id)initWithWebFrame:(WebFrame *)webFrame;
- (void)close;

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
- (void)runOpenPanelForFileButtonWithResultListener:(id <WebOpenPanelResultListener>)resultListener;
- (BOOL)inNextKeyViewOutsideWebFrameViews;

@end
