/*
    WebNSEventExtras.m
    Copyright (c) 2003, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNSEventExtras.h>

@implementation NSEvent (WebExtras)

-(BOOL)_web_isTabKeyEvent
{
    if ([self type] != NSKeyDown) {
        return NO;
    }
    
    NSString *chars = [self charactersIgnoringModifiers];
    if ([chars length] != 1)
        return NO;
    
    const unichar tabKey = 0x0009;
    const unichar shiftTabKey = 0x0019;
    unichar c = [chars characterAtIndex:0];
    if (c != tabKey && c != shiftTabKey)
        return NO;
    
    return YES;
}

@end
