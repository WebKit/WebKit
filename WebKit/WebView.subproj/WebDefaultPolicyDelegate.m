/*	
        WebDefaultPolicyDelegate.m
	Copyright 2002, Apple Computer, Inc.
*/
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <WebKit/WebAssertions.h>


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

- (void)webView: (WebView *)wv unableToImplementPolicyWithError:(NSError *)error frame:(WebFrame *)frame
{
    ERROR("called unableToImplementPolicyWithError:%@ inFrame:%@", error, frame);
}


- (void)webView: (WebView *)wv decidePolicyForMIMEType:(NSString *)type
                                               request:(NSURLRequest *)request
                                                 frame:(WebFrame *)frame
		                      decisionListener:(WebPolicyDecisionListener *)listener;
{
    if ([[request URL] isFileURL]) {
	BOOL isDirectory;
	[[NSFileManager defaultManager] fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];
	
	if (isDirectory) {
	    [listener ignore];
	} else if([WebView canShowMIMEType:type]) {
	    [listener use];
	} else{
	    [listener ignore];
	}
    } else if ([WebView canShowMIMEType:type]) {
        [listener use];
    } else {
        [listener ignore];
    }
}

- (void)webView: (WebView *)wv decidePolicyForNavigationAction:(NSDictionary *)actionInformation 
                                                       request:(NSURLRequest *)request
                                                         frame:(WebFrame *)frame
                                              decisionListener:(WebPolicyDecisionListener *)listener
{
    WebNavigationType navType = [[actionInformation objectForKey:WebActionNavigationTypeKey] intValue];

    if ([WebView _canHandleRequest:request]) {
	[listener use];
    } else if (navType == WebNavigationTypePlugInRequest) {
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

- (void)webView: (WebView *)wv decidePolicyForNewWindowAction:(NSDictionary *)actionInformation 
                                                      request:(NSURLRequest *)request
                                                 newFrameName:(NSString *)frameName
                                             decisionListener:(WebPolicyDecisionListener *)listener
{
    [listener use];
}

// Temporary SPI needed for <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
- (BOOL)webView:(WebView *)webView shouldGoToHistoryItem:(WebHistoryItem *)item
{
    return YES;
}

@end

