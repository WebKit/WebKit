/*
    WebNSEventExtras.h
    Copyright (c) 2003, Apple, Inc. All rights reserved.
 */

#import <AppKit/AppKit.h>

@interface NSEvent (WebExtras)

-(BOOL)_web_isKeyEvent:(unichar)key;

-(BOOL)_web_isDeleteKeyEvent;
-(BOOL)_web_isEscapeKeyEvent;
-(BOOL)_web_isOptionTabKeyEvent;
-(BOOL)_web_isReturnOrEnterKeyEvent;
-(BOOL)_web_isTabKeyEvent;

@end
