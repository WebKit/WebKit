/*
    WebNSEventExtras.m
    Copyright (c) 2003, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNSEventExtras.h>

@implementation NSEvent (WebExtras)

-(BOOL)_web_isKeyEvent:(unichar)key
{
    int type = [self type];
    if (type != NSKeyDown && type != NSKeyUp)
        return NO;
    
    NSString *chars = [self charactersIgnoringModifiers];
    if ([chars length] != 1)
        return NO;
    
    unichar c = [chars characterAtIndex:0];
    if (c != key)
        return NO;
    
    return YES;
}

- (BOOL)_web_isDeleteKeyEvent
{
    const unichar deleteKey = NSDeleteCharacter;
    const unichar deleteForwardKey = NSDeleteFunctionKey;
    return [self _web_isKeyEvent:deleteKey] || [self _web_isKeyEvent:deleteForwardKey];
}

- (BOOL)_web_isEscapeKeyEvent
{
    const unichar escapeKey = 0x001b;
    return [self _web_isKeyEvent:escapeKey];
}

- (BOOL)_web_isOptionTabKeyEvent
{
    return ([self modifierFlags] & NSAlternateKeyMask) && [self _web_isTabKeyEvent];
}

- (BOOL)_web_isReturnOrEnterKeyEvent
{
    const unichar enterKey = NSEnterCharacter;
    const unichar returnKey = NSCarriageReturnCharacter;
    return [self _web_isKeyEvent:enterKey] || [self _web_isKeyEvent:returnKey];
}

- (BOOL)_web_isTabKeyEvent
{
    const unichar tabKey = 0x0009;
    const unichar shiftTabKey = 0x0019;
    return [self _web_isKeyEvent:tabKey] || [self _web_isKeyEvent:shiftTabKey];
}

@end
