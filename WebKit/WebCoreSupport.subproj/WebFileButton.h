//
//  WebFileButton.h
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol WebCoreFileButton;

@interface WebFileButton : NSView <WebCoreFileButton>
{
    NSString *_filename;
    NSButton *_button;
    NSImage *_icon;
    NSString *_label;
}
@end
