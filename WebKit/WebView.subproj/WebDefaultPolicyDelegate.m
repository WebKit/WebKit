/*	
        WebDefaultPolicyDelegate.m
	Copyright 2002, Apple Computer, Inc.
*/
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegatePrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>


@implementation WebDefaultPolicyDelegate

static WebDefaultPolicyDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultPolicyDelegate *)_sharedWebPolicyDelegate
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
				 andRequest:(WebResourceRequest *)request
				    inFrame:(WebFrame *)frame;
{
    if ([[request URL] isFileURL]) {
	BOOL isDirectory;
	[[NSFileManager defaultManager] fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];
	
	if(isDirectory)
	    return WebPolicyIgnore;
	if([WebController canShowMIMEType:type])
	    return WebPolicyShow;
	return WebPolicyIgnore;
    }

    if ([WebController canShowMIMEType:type]) {
        return WebPolicyShow;
    } else {
        return WebPolicyIgnore;
    }
}

- (NSString *)savePathForResponse:(WebResourceResponse *)response
                       andRequest:(WebResourceRequest *)request
{
    return nil;
}

- (void)decideNavigationPolicyForAction:(NSDictionary *)actionInformation 
			     andRequest:(WebResourceRequest *)request
				inFrame:(WebFrame *)frame
		       decisionListener:(WebPolicyDecisionListener *)listener
{
    if ([WebResourceHandle canInitWithRequest:request]) {
	[listener usePolicy:WebPolicyUse];
    }else{
        [listener usePolicy:WebPolicyOpenURL];
    }
}

@end

