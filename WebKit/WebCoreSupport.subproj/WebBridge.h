//
//  WebBridge.h
//  WebKit
//
//  Created by Darin Adler on Thu Jun 13 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreBridge.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebWindowOperationsDelegate.h>

@interface WebBridge : WebCoreBridge <WebCoreBridge>
{
    WebFrame *frame;
    BOOL _doingClientRedirect;
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
- (void)setFrame:(WebFrame *)webFrame;
- (void)runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener;

@end
