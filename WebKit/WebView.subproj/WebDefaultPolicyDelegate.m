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

- (WebURLPolicy)URLPolicyForURL: (NSURL *)url
{
    return [WebController defaultURLPolicyForURL: url];
}

- (WebFileURLPolicy)fileURLPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource isDirectory:(BOOL)isDirectory
{
    if(isDirectory)
        return WebFileURLPolicyIgnore;
    if([WebController canShowMIMEType:type])
        return WebFileURLPolicyUseContentPolicy;
    return WebFileURLPolicyIgnore;
}

- (void)unableToImplementFileURLPolicy: (WebError *)error forDataSource: (WebDataSource *)dataSource
{
    NSLog (@"unableToImplementFileURLPolicy:forDataSource: - error %@\n", error);
}

- (void)requestContentPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource
{
    if([WebController canShowMIMEType:type]){
        [webController haveContentPolicy:WebContentPolicyShow andPath:nil forDataSource:dataSource];
    }
    else{
        [webController haveContentPolicy:WebContentPolicyIgnore andPath:nil forDataSource:dataSource];
    }
}

- (void)unableToImplementURLPolicyForURL: (NSURL *)url error: (WebError *)error
{
    NSLog (@"unableToImplementURLPolicyForURL:error: - URL %@, error %@\n", url, error);
}


- (void)unableToImplementContentPolicy: (WebError *)error forDataSource: (WebDataSource *)dataSource
{
    NSLog (@"unableToImplementContentPolicy:forDataSource: - error %@\n", error);
}

- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)url
{
    NSLog (@"pluginNotFoundForMIMEType:pluginPageURL: - MIME %@, URL \n", mime, url);
}

@end

