/*	IFBaseLocationChangeHander.m

        Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFBaseLocationChangeHandler.h>


@implementation IFBaseLocationChangeHandler

+ (void)setGlobalContentPolicy: (IFContentPolicy)policy forMIMEType: (NSString *)type
{
}

+ (IFContentPolicy)globaContentPolicyForContentType: (NSString *)type
{
    return IFContentPolicyShow;
}

+ (NSDictionary *)globalContentPolicies
{
}

+ (NSString *)suggestedFileanemForURL: (NSURL *) andContentType: (IFContentType *)type
{
}

+ (NSString *)suggestedDirectoryForURL: (NSURL *) andContentType: (IFContentType *)type
{
}

+ (NSString *)extensionForURL: (NSURL *)url
{
}



// Returns the extension from the URL.  May be used in conjunction with 
// the MIME type to determine how a location should be handled.
- (NSString *)extension
{
    return nil;
}

- (BOOL)locationWillChangeTo: (NSURL *)url
{
    url = [url retain];
}


- (void)locationChangeStarted
{
    // Do nothing.  Subclasses may override.
}


- (void)locationChangeCommitted
{
    // Do nothing.  Subclasses may override.
}


- (void)locationChangeDone: (IFError *)error
{
    // Do nothing.  Subclasses may override.
}

- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses may override.
}

- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses may override.
}

// DEPRECATED: 
// Called when a file download has started
- (void) downloadingWithHandler:(IFDownloadHandler *)downloadHandler
{
}


// Sent once the IFContentType of the location handler
// has been determined.  Should not block.
// Implementations typically call setContentPolicy: immediately, although
// may call it later after showing a user dialog.
- (void)requestContentPolicyForContentMIMEType: (NSString *)type
{
    [self haveContentPolicy: [IFBaseLocationChangeHandler globaContentPolicyForContentType: type] forLocationChangeHandler: self];
}

// We may have different errors that cause the the policy to be un-implementable, i.e.
// file i/o failure, launch services failure, type mismatches, etc.
- (void)unableToImplementContentPolicy: (IFError *)error
{
}


@end

