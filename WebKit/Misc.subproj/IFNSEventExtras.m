/*
    IFNSEventExtras.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import "IFNSEventExtras.h"


@implementation NSEvent (IFExtensions)

- (BOOL) _IF_isScrollEvent
{
    NSEventType eventType = [self type];

    if (eventType == NSKeyDown || eventType == NSKeyUp) {
        NSString *characters = [self characters];
        int index, count;

        count = [characters length];
        for (index = 0; index < count; ++index) {
            switch ([characters characterAtIndex:index]) {
                case NSPageUpFunctionKey:
                case NSPageDownFunctionKey:
                case NSHomeFunctionKey:
                case NSEndFunctionKey:
                case NSUpArrowFunctionKey:
                case NSDownArrowFunctionKey:
                case NSLeftArrowFunctionKey:
                case NSRightArrowFunctionKey:
                    return YES;
                default: break;
            }
        }
    }
    
    return NO;
}

@end
