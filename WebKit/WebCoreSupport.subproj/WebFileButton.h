//
//  WebFileButton.h
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol WebCoreFileButton;
@protocol WebCoreFileButtonDelegate;
@protocol WebOpenPanelResultListener;
@class WebBridge;
@class WebFileChooserButton;

@interface WebFileButton : NSView <WebCoreFileButton, WebOpenPanelResultListener>
{
    NSString *_filename;
    WebFileChooserButton *_button;
    NSImage *_icon;
    NSString *_label;
    WebBridge *_bridge;
    BOOL _inNextValidKeyView;
    id <WebCoreFileButtonDelegate> _delegate;
}
- (id)initWithBridge:(WebBridge *)bridge delegate:(id <WebCoreFileButtonDelegate>)delegate;
@end
