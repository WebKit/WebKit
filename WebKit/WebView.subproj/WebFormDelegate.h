/*
        WebFormDelegate.h
        Copyright 2003, Apple Computer, Inc.
*/

#import <AppKit/AppKit.h>

@class DOMElement;
@class WebFrame;

/*!
    @protocol  WebFormSubmissionListener
    @discussion .
*/
@protocol WebFormSubmissionListener <NSObject>
- (void)continue;
@end

/*!
    @protocol  WebFormDelegate
    @discussion .
*/
@protocol WebFormDelegate <NSObject>

// Various methods send by controls that edit text to their delegates, which are all
// analogous to similar methods in AppKit/NSControl.h.
// These methods are forwarded from widgets used in forms to the WebFormDelegate.

- (void)controlTextDidBeginEditing:(NSNotification *)obj inFrame:(WebFrame *)frame;
- (void)controlTextDidEndEditing:(NSNotification *)obj inFrame:(WebFrame *)frame;
- (void)controlTextDidChange:(NSNotification *)obj inFrame:(WebFrame *)frame;
- (void)textDidChange:(NSNotification *)obj inFrame:(WebFrame *)frame;

- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor inFrame:(WebFrame *)frame;
- (BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor inFrame:(WebFrame *)frame;
- (BOOL)control:(NSControl *)control didFailToFormatString:(NSString *)string errorDescription:(NSString *)error inFrame:(WebFrame *)frame;
- (void)control:(NSControl *)control didFailToValidatePartialString:(NSString *)string errorDescription:(NSString *)error inFrame:(WebFrame *)frame;
- (BOOL)control:(NSControl *)control isValidObject:(id)obj inFrame:(WebFrame *)frame;

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector inFrame:(WebFrame *)frame;
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView shouldHandleEvent:(NSEvent *)event inFrame:(WebFrame *)frame;

// Sent when a form is just about to be submitted (before the load is started)
// listener must be sent continue when the delegate is done.
- (void)frame:(WebFrame *)frame sourceFrame:(WebFrame *)sourceFrame willSubmitForm:(DOMElement *)form withValues:(NSDictionary *)values submissionListener:(id <WebFormSubmissionListener>)listener;
@end

/*!
    @class WebFormDelegate
    @discussion The WebFormDelegate class responds to all WebFormDelegate protocol
    methods by doing nothing. It's provided for the convenience of clients who only want
    to implement some of the above methods and ignore others.
*/
@interface WebFormDelegate : NSObject <WebFormDelegate>
{
}
@end

