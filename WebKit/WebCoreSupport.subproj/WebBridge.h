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
    WebCoreKeyboardUIMode _keyboardUIMode;
    BOOL _keyboardUIModeAccessed;
    BOOL _doingClientRedirect;
    BOOL _inNextKeyViewOutsideWebFrameViews;
    BOOL _haveUndoRedoOperations;
    
    NSDictionary *lastDashboardRegions;
}

- (id)initWithWebFrame:(WebFrame *)webFrame;
- (void)close;

- (void)receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName;
- (void)runOpenPanelForFileButtonWithResultListener:(id <WebOpenPanelResultListener>)resultListener;
- (BOOL)inNextKeyViewOutsideWebFrameViews;

- (WebFrame *)webFrame;

@end
