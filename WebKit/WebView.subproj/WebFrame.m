/*	
        WebFrame.m
        Copyright (c) 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrame.h>

#import <Cocoa/Cocoa.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <Foundation/NSURLRequest.h>

#import <Foundation/NSString_NSURLExtras.h>

@implementation WebFrame

- init
{
    return [self initWithName:nil webFrameView:nil webView:nil];
}

- initWithName:(NSString *)n webFrameView:(WebFrameView *)fv webView:(WebView *)v
{
    [super init];

    _private = [[WebFramePrivate alloc] init];

    [self _setWebView:v];
    [self _setName:n];

    _private->bridge = [[WebBridge alloc] initWithWebFrame:self];
    
    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebView:v];
    }
    
    ++WebFrameCount;
    
    return self;
}

- (void)dealloc
{
    --WebFrameCount;

    [self _detachFromParent];
    [_private release];
    [super dealloc];
}

- (NSString *)name
{
    return [_private name];
}

- (WebFrameView *)frameView
{
    return [_private webFrameView];
}

- (WebView *)webView
{
    return [_private webView];
}


- (WebDataSource *)provisionalDataSource
{
    return [_private provisionalDataSource];
}


- (WebDataSource *)dataSource
{
    return [_private dataSource];
}

- (void)loadRequest:(NSURLRequest *)request
{
    WebFrameLoadType loadType;

    // note this copies request
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    NSMutableURLRequest *r = [newDataSource request];
    [self _addExtraFieldsToRequest:r alwaysFromRequest: NO];
    if ([self _shouldTreatURLAsSameAsCurrent:[request URL]]) {
        [r setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        loadType = WebFrameLoadTypeSame;
    } else {
        loadType = WebFrameLoadTypeStandard;
    }

    [newDataSource _setOverrideEncoding:[[self dataSource] _overrideEncoding]];

    [self _loadDataSource:newDataSource withLoadType:loadType formState:nil];
    [newDataSource release];
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName: (NSString *)encodingName baseURL:(NSURL *)URL;
{
    NSURL *fakeURL = [NSURLRequest _webDataRequestURLForData: data];
    NSMutableURLRequest *request = [[[NSMutableURLRequest alloc] initWithURL: fakeURL] autorelease];
    [request _webDataRequestSetData:data];
    [request _webDataRequestSetEncoding:encodingName];
    [request _webDataRequestSetBaseURL:URL];
    [request _webDataRequestSetMIMEType:MIMEType?MIMEType:@"text/html"];
    [self loadRequest:request];
}

- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)URL
{
    CFStringEncoding cfencoding = CFStringGetFastestEncoding((CFStringRef)string);
    NSStringEncoding nsencoding = CFStringConvertEncodingToNSStringEncoding(cfencoding);
    CFStringRef cfencodingName = CFStringConvertEncodingToIANACharSetName(cfencoding);
    
    if (!cfencodingName || nsencoding == kCFStringEncodingInvalidId){
        NSData *data = [string dataUsingEncoding: NSUnicodeStringEncoding];
        [self loadData:data MIMEType:nil textEncodingName:@"utf-16" baseURL:URL];
    }
    else {
        NSData *data = [string dataUsingEncoding: nsencoding];
        [self loadData:data MIMEType:nil textEncodingName:(NSString *)cfencodingName baseURL:URL];
    }
}

- (void)stopLoading
{
    [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    [_private->provisionalDataSource _stopLoading];
    [_private->dataSource _stopLoading];
    [_private->scheduledLayoutTimer invalidate];
    _private->scheduledLayoutTimer = nil;

    // Release the provisional data source because there's no point in keeping it around since it is unused in this case.
    [self _setProvisionalDataSource:nil];
}


- (void)reload
{
    WebDataSource *dataSource = [self dataSource];
    if (dataSource == nil) {
	return;
    }

    // initWithRequest copies the request
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:[dataSource request]];
    NSMutableURLRequest *request = [newDataSource request];
    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    // If we're about to rePOST, set up action so the app can warn the user
    if ([[request HTTPMethod] _web_isCaseInsensitiveEqualToString:@"POST"]) {
        NSDictionary *action = [self _actionInformationForNavigationType:WebNavigationTypeFormResubmitted event:nil originalURL:[request URL]];
        [newDataSource _setTriggeringAction:action];
    }

    [newDataSource _setOverrideEncoding:[dataSource _overrideEncoding]];
    
    [self _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReload formState:nil];

    [newDataSource release];
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    // First, deal with 'special' names.
    if ([name isEqualToString:@"_self"] || [name isEqualToString:@"_current"]){
        return self;
    }
    
    if ([name isEqualToString:@"_top"]) {
        return [[self webView] mainFrame];
    }
    
    if ([name isEqualToString:@"_parent"]) {
        WebFrame *parent = [self parentFrame];
        return parent ? parent : self;
    }
    
    if ([name isEqualToString:@"_blank"]) {
        return nil;
    }

    // Search from this frame down.
    WebFrame *frame = [self _descendantFrameNamed:name];

    if (!frame) {
        // Search in this WebView then in others.
        frame = [[self webView] _findFrameNamed:name];
    }

    return frame;
}

- (WebFrame *)parentFrame
{
    return [[_private->parent retain] autorelease];
}

- (NSArray *)childFrames
{
    return [[_private->children copy] autorelease];
}

@end
