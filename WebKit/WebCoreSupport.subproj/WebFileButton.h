//
//  WebFileButton.h
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol WebCoreFileButton;
@protocol WebOpenPanelResultListener;
@class WebBridge;

@interface WebFileButton : NSView <WebCoreFileButton, WebOpenPanelResultListener>
{
    NSString *_filename;
    NSButton *_button;
    NSImage *_icon;
    NSString *_label;
    WebBridge *_bridge;
}
- (id)initWithBridge:(WebBridge *)bridge;
@end
