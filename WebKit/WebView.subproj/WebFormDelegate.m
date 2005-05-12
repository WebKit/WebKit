/*
        WebFormDelegate.m
	Copyright 2003, Apple, Inc. All rights reserved.
 */

#import "WebFormDelegatePrivate.h"

//FIXME:  This should become an informal protocol, now that we switch all the others

@implementation WebFormDelegate

static WebFormDelegate *sharedDelegate = nil;

// Return a object with NOP implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebFormDelegate *)_sharedWebFormDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebFormDelegate alloc] init];
    }
    return sharedDelegate;
}
    
- (void)controlTextDidBeginEditing:(NSNotification *)obj inFrame:(WebFrame *)frame { }

- (void)controlTextDidEndEditing:(NSNotification *)obj inFrame:(WebFrame *)frame { }

- (void)controlTextDidChange:(NSNotification *)obj inFrame:(WebFrame *)frame { }

- (void)textDidChange:(NSNotification *)obj inFrame:(WebFrame *)frame { }

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

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView shouldHandleEvent:(NSEvent *)event inFrame:(WebFrame *)frame
{
    return NO;
}

- (void)frame:(WebFrame *)frame sourceFrame:(WebFrame *)sourceFrame willSubmitForm:(DOMElement *)form withValues:(NSDictionary *)values submissionListener:(id <WebFormSubmissionListener>)listener
{
    [listener continue];
}

@end
