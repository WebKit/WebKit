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

- (WebURLPolicy *)URLPolicyForURL: (NSURL *)URL
{
    return [WebController defaultURLPolicyForURL: URL];
}

- (WebFileURLPolicy *)fileURLPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource isDirectory:(BOOL)isDirectory
{
    if(isDirectory)
        return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyIgnore];
    if([WebController canShowMIMEType:type])
        return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyUseContentPolicy];
    return [WebFileURLPolicy webPolicyWithFileAction:WebFileURLPolicyIgnore];;
}

- (void)unableToImplementFileURLPolicy: (WebPolicy *)policy error: (WebError *)error forDataSource: (WebDataSource *)dataSource
{
    NSLog (@"unableToImplementFileURLPolicy:forDataSource: - error %@\n", error);
}

- (WebContentPolicy *)contentPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource;
{
    if([WebController canShowMIMEType:type]){
        return [WebContentPolicy webPolicyWithContentAction: WebContentPolicyShow andPath:nil];
    }
    else{
        return [WebContentPolicy webPolicyWithContentAction: WebContentPolicyIgnore andPath:nil];
    }
}

- (void)unableToImplementURLPolicy: (WebPolicy *)policy error: (WebError *)error forURL: (NSURL *)URL
{
    NSLog (@"unableToImplementURLPolicyForURL:error: - URL %@, error %@\n", URL, error);
}


- (void)unableToImplementContentPolicy: (WebPolicy *)policy error: (WebError *)error forDataSource: (WebDataSource *)dataSource;
{
    NSLog (@"unableToImplementContentPolicy:forDataSource: - error %@\n", error);
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

