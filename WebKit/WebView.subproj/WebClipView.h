//
//  WebClipView.h
//  WebKit
//
//  Created by Darin Adler on Tue Sep 24 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <AppKit/AppKit.h>

@interface WebClipView : NSClipView
{
    BOOL _haveAdditionalClip;
    NSRect _additionalClip;
}

- (void)setAdditionalClip:(NSRect)additionalClip;
- (void)resetAdditionalClip;
- (BOOL)hasAdditionalClip;
- (NSRect)additionalClip;

@end
