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

- (void)unableToImplementPolicy:(WebPolicyAction)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame
{
    NSLog (@"called unableToImplementPolicy:%derror:%@:inFrame:%@", policy, error, frame);
}


- (WebPolicyAction)contentPolicyForMIMEType:(NSString *)type
				 andRequest:(WebRequest *)request
				    inFrame:(WebFrame *)frame;
{
    if ([[request URL] isFileURL]) {
	BOOL isDirectory;
	[[NSFileManager defaultManager] fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];
	
	if(isDirectory)
	    return WebPolicyIgnore;
	if([WebContentTypes canShowMIMEType:type])
	    return WebPolicyUse;
	return WebPolicyIgnore;
    }

    if ([WebContentTypes canShowMIMEType:type]) {
        return WebPolicyUse;
    } else {
        return WebPolicyIgnore;
    }
}

- (void)decideNavigationPolicyForAction:(NSDictionary *)actionInformation 
			     andRequest:(WebRequest *)request
				inFrame:(WebFrame *)frame
		       decisionListener:(WebPolicyDecisionListener *)listener
{
    if ([WebResource canInitWithRequest:request]) {
	[listener usePolicy:WebPolicyUse];
    } else {
	// A file URL shouldn't fall through to here, but if it did,
	// it would be a security risk to open it.
	if (![[request URL] isFileURL]) {
	    [[NSWorkspace sharedWorkspace] openURL:[request URL]];
	}
        [listener usePolicy:WebPolicyIgnore];
    }
}

- (void)decideNewWindowPolicyForAction:(NSDictionary *)actionInformation 
			     andRequest:(WebRequest *)request
			   newFrameName:(NSString *)frameName
		       decisionListener:(WebPolicyDecisionListener *)listener
{
    [listener usePolicy:WebPolicyUse];
}

@end

