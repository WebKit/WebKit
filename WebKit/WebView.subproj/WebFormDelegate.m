/*
        WebFormDelegate.m
	Copyright 2003, Apple, Inc. All rights reserved.
 */

#import "WebFormDelegate.h"

@implementation WebFormDelegate

- (void)controlTextDidBeginEditing:(NSNotification *)obj inFrame:(WebFrame *)frame { }

- (void)controlTextDidEndEditing:(NSNotification *)obj inFrame:(WebFrame *)frame { }

- (void)controlTextDidChange:(NSNotification *)obj inFrame:(WebFrame *)frame { }


- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor inFrame:(WebFrame *)frame
{
    return YES;
}

- (BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor inFrame:(WebFrame *)frame
{
    return YES;
}

- (BOOL)control:(NSControl *)control didFailToFormatString:(NSString *)string errorDescription:(NSString *)error inFrame:(WebFrame *)frame
{
    return YES;
}

- (void)control:(NSControl *)control didFailToValidatePartialString:(NSString *)string errorDescription:(NSString *)error inFrame:(WebFrame *)frame
{
}

- (BOOL)control:(NSControl *)control isValidObject:(id)obj inFrame:(WebFrame *)frame
{
    return YES;
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector inFrame:(WebFrame *)frame
{
    return NO;
}

@end
