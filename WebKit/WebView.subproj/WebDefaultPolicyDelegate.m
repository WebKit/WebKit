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

+ (WebURLPolicy *)defaultURLPolicyForRequest: (WebResourceRequest *)request
{
    if([WebResourceHandle canInitWithRequest:request]){
        return [WebURLPolicy webPolicyWithURLAction:WebURLPolicyUseContentPolicy];
    }else{
        return [WebURLPolicy webPolicyWithURLAction:WebURLPolicyOpenExternally];
    }
}


- initWithWebController: (WebController *)wc
{
    [super init];
    webController = wc;  // Non-retained, like a delegate.
    return self;
}

- (WebURLPolicy *)URLPolicyForRequest:(WebResourceRequest *)request inFrame:(WebFrame *)frame
{
    return [WebDefaultPolicyDelegate defaultURLPolicyForRequest:request];
}

- (WebFileURLPolicy *)fileURLPolicyForMIMEType:(NSString *)type andRequest:(WebResourceRequest *)request inFrame:(WebFrame *)frame
{
    BOOL isDirectory;
    [[NSFileManager defaultManager] fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];

    if(isDirectory)
        return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyIgnore];
    if([WebController canShowMIMEType:type])
        return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyUseContentPolicy];
    return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyIgnore];
}

- (void)unableToImplementPolicy:(WebPolicy *)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame
{
    NSLog (@"called unableToImplementPolicy:%derror:%@:inFrame:%@", policy, error, frame);
}


- (WebContentPolicy *)contentPolicyForResponse:(WebResourceResponse *)response
				    andRequest:(WebResourceRequest *)request
                                       inFrame:(WebFrame *)frame;
{
    if ([WebController canShowMIMEType:[response contentType]]) {
        return [WebContentPolicy webPolicyWithContentAction:WebContentPolicyShow];
    } else {
        return [WebContentPolicy webPolicyWithContentAction:WebContentPolicyIgnore];
    }
}

- (NSString *)saveFilenameForResponse:(WebResourceResponse *)response
                           andRequest:(WebResourceRequest *)request
{
    return nil;
}

- (WebClickPolicy *)clickPolicyForAction:(NSDictionary *)actionInformation 
			      andRequest:(WebResourceRequest *)request
				 inFrame:(WebFrame *)frame
{
    return [WebClickPolicy webPolicyWithClickAction:WebClickPolicyShow];
}

@end

