/*	
        WebDefaultPolicyDelegate.m
	Copyright 2002, Apple Computer, Inc.
*/
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebResourceResponse.h>


@implementation WebDefaultPolicyDelegate

- initWithWebController: (WebController *)wc
{
    [super init];
    webController = wc;  // Non-retained, like a delegate.
    return self;
}

- (WebFileAction)fileURLPolicyForMIMEType:(NSString *)type andRequest:(WebResourceRequest *)request inFrame:(WebFrame *)frame
{
    BOOL isDirectory;
    [[NSFileManager defaultManager] fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];

    if(isDirectory)
        return WebFileURLPolicyIgnore;
    if([WebController canShowMIMEType:type])
        return WebFileURLPolicyUseContentPolicy;
    return WebFileURLPolicyIgnore;
}

- (void)unableToImplementPolicy:(WebPolicyAction)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame
{
    NSLog (@"called unableToImplementPolicy:%derror:%@:inFrame:%@", policy, error, frame);
}


- (WebPolicyAction)contentPolicyForResponse:(WebResourceResponse *)response
				    andRequest:(WebResourceRequest *)request
                                       inFrame:(WebFrame *)frame;
{
    if ([WebController canShowMIMEType:[response contentType]]) {
        return WebContentPolicyShow;
    } else {
        return WebContentPolicyIgnore;
    }
}

- (NSString *)saveFilenameForResponse:(WebResourceResponse *)response
                           andRequest:(WebResourceRequest *)request
{
    return nil;
}

- (WebPolicyAction)navigationPolicyForAction:(NSDictionary *)actionInformation 
				  andRequest:(WebResourceRequest *)request
				     inFrame:(WebFrame *)frame
{
    if([WebResourceHandle canInitWithRequest:request]){
        return WebPolicyUse;
    }else{
        return WebPolicyOpenURL;
    }
}

@end

