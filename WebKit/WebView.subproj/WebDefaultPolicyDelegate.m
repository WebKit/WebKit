/*	
        WebDefaultPolicyDelegate.m
	Copyright 2002, Apple Computer, Inc.
*/
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegatePrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResponse.h>
#import <WebFoundation/WebAssertions.h>


@implementation WebDefaultPolicyDelegate

static WebDefaultPolicyDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultPolicyDelegate *)sharedPolicyDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultPolicyDelegate alloc] init];
    }
    return sharedDelegate;
}

- (void)controller:(WebController *)c unableToImplementPolicyWithError:(WebError *)error inFrame:(WebFrame *)frame
{
    ERROR("called unableToImplementPolicyWithError:%@ inFrame:%@", error, frame);
}


- (void)controller:(WebController *)c decideContentPolicyForMIMEType:(NSString *)type
				 andRequest:(WebRequest *)request
				    inFrame:(WebFrame *)frame
		           decisionListener:(WebPolicyDecisionListener *)listener;
{
    if ([[request URL] isFileURL]) {
	BOOL isDirectory;
	[[NSFileManager defaultManager] fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];
	
	if (isDirectory) {
	    [listener ignore];
	} else if([WebContentTypes canShowMIMEType:type]) {
	    [listener use];
	} else{
	    [listener ignore];
	}
    } else if ([WebContentTypes canShowMIMEType:type]) {
        [listener use];
    } else {
        [listener ignore];
    }
}

- (void)controller:(WebController *)c decideNavigationPolicyForAction:(NSDictionary *)actionInformation 
			     andRequest:(WebRequest *)request
				inFrame:(WebFrame *)frame
		       decisionListener:(WebPolicyDecisionListener *)listener
{
    if ([WebResource canInitWithRequest:request]) {
	[listener use];
    } else {
	// A file URL shouldn't fall through to here, but if it did,
	// it would be a security risk to open it.
	if (![[request URL] isFileURL]) {
	    [[NSWorkspace sharedWorkspace] openURL:[request URL]];
	}
        [listener ignore];
    }
}

- (void)controller:(WebController *)c decideNewWindowPolicyForAction:(NSDictionary *)actionInformation 
			     andRequest:(WebRequest *)request
			   newFrameName:(NSString *)frameName
		       decisionListener:(WebPolicyDecisionListener *)listener
{
    [listener use];
}

@end

