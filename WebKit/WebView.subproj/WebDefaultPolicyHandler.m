/*	
        WebDefaultPolicyHandler.h
	Copyright 2002, Apple Computer, Inc.

        Public header file.
*/
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultPolicyHandler.h>
#import <WebKit/WebFrame.h>


@implementation WebDefaultPolicyHandler

- initWithWebController: (WebController *)wc
{
    [super init];
    webController = wc;  // Non-retained, like a delegate.
    return self;
}

- (WebURLPolicy *)URLPolicyForURL:(NSURL *)URL inFrame:(WebFrame *)frame
{
    return [WebController defaultURLPolicyForURL:URL];
}

- (WebFileURLPolicy *)fileURLPolicyForMIMEType:(NSString *)type inFrame:(WebFrame *)frame isDirectory:(BOOL)isDirectory
{
    if(isDirectory)
        return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyIgnore];
    if([WebController canShowMIMEType:type])
        return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyUseContentPolicy];
    return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyIgnore];;
}

- (void)unableToImplementPolicy:(WebPolicy *)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame
{
    NSLog (@"called unableToImplementPolicy:%derror:%@:inFrame:%@", policy, error, frame);
}


- (WebContentPolicy *)contentPolicyForMIMEType: (NSString *)type URL:(NSURL *)URL inFrame:(WebFrame *)frame;
{
    if([WebController canShowMIMEType:type]){
        return [WebContentPolicy webPolicyWithContentAction: WebContentPolicyShow andPath:nil];
    }
    else{
        return [WebContentPolicy webPolicyWithContentAction: WebContentPolicyIgnore andPath:nil];
    }
}

- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)URL
{
    NSLog (@"pluginNotFoundForMIMEType:pluginPageURL: - MIME %@, URL \n", mime, URL);
}

- (WebClickPolicy *)clickPolicyForElement: (NSDictionary *)elementInformation button: (NSEventType)eventType modifierMask: (unsigned int)eventMask
{
    return [WebClickPolicy webPolicyWithClickAction:WebClickPolicyShow URL:nil andPath:nil];
}

@end

