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

+ (WebURLPolicy *)defaultURLPolicyForURL: (NSURL *)URL
{
    if([WebResourceHandle canInitWithRequest:[WebResourceRequest requestWithURL:URL]]){
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

- (WebURLPolicy *)URLPolicyForURL:(NSURL *)URL inFrame:(WebFrame *)frame
{
    return [WebDefaultPolicyDelegate defaultURLPolicyForURL:URL];
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
                                       inFrame:(WebFrame *)frame
                             withContentPolicy:(WebContentPolicy *)contentPolicy;
{
    if([WebController canShowMIMEType:[response contentType]]){
        return [WebContentPolicy webPolicyWithContentAction: WebContentPolicyShow andPath:nil];
    }
    else{
        return [WebContentPolicy webPolicyWithContentAction: WebContentPolicyIgnore andPath:nil];
    }
}

- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)URL
{
    NSLog (@"pluginNotFoundForMIMEType:pluginPageURL: - MIME %@, URL ", mime, URL);
}

- (WebClickPolicy *)clickPolicyForElement: (NSDictionary *)elementInformation button: (NSEventType)eventType modifierFlags: (unsigned int)modifierFlags
{
    return [WebClickPolicy webPolicyWithClickAction:WebClickPolicyShow URL:nil andPath:nil];
}

@end

