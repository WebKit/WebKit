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
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebProtocol.h>

#import <WebFoundation/WebNSStringExtras.h>

@implementation WebFrame

- init
{
    return [self initWithName: nil webFrameView: nil webView: nil];
}

- initWithName: (NSString *)n webFrameView: (WebFrameView *)v webView: (WebView *)c
{
    [super init];

    _private = [[WebFramePrivate alloc] init];

    [self setController:c];

    _private->bridge = [[WebBridge alloc] init];
    [_private->bridge initializeSettings: [c _settings]];
    [_private->bridge setWebFrame:self];
    [_private->bridge setName:n];

    [self _setName:n];
    
    if (v) {
        [_private setWebFrameView: v];
        [v _setController: [self webView]];
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
    return [_private controller];
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

- (void)loadData:(NSData *)data encodingName: (NSString *)encodingName baseURL:(NSURL *)URL;
{
    NSURL *fakeURL = [NSURLRequest _webDataRequestURLForData: data];
    NSURLRequest *request = [[[NSURLRequest alloc] initWithURL: fakeURL] autorelease];
    [request _webDataRequestSetData:data];
    [request _webDataRequestSetEncoding:encodingName];
    [request _webDataRequestSetBaseURL:URL];
    [self loadRequest:request];
}

- (void)loadString:(NSString *)string baseURL:(NSURL *)URL
{
    CFStringEncoding cfencoding = CFStringGetFastestEncoding((CFStringRef)string);
    NSStringEncoding nsencoding = CFStringConvertEncodingToNSStringEncoding(cfencoding);
    CFStringRef cfencodingName = CFStringConvertEncodingToIANACharSetName(cfencoding);
    
    if (!cfencodingName || nsencoding == kCFStringEncodingInvalidId){
        NSData *data = [string dataUsingEncoding: NSUnicodeStringEncoding];
        [self loadData:data encodingName:@"utf-16" baseURL:URL];
    }
    else {
        NSData *data = [string dataUsingEncoding: nsencoding];
        [self loadData:data encodingName:(NSString *)cfencodingName baseURL:URL];
    }
}

- (void)stopLoading
{
    [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    if (_private->state != WebFrameStateComplete) {
        [_private->provisionalDataSource _stopLoading];
        [_private->dataSource _stopLoading];
        [_private->scheduledLayoutTimer fire];
    }
    ASSERT(_private->scheduledLayoutTimer == nil);
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
        WebFrame *parent = [self parent];
        return parent ? parent : self;
    }
    
    if ([name isEqualToString:@"_blank"]) {
        return nil;
    }

    // Search from this frame down.
    WebFrame *frame = [self _descendantFrameNamed:name];

    if(!frame){
        // Search in this controller then in other controllers.
        frame = [[self webView] _findFrameNamed:name];
    }

    return frame;
}

- (WebFrame *)parent
{
    return [[_private->parent retain] autorelease];
}

- (NSArray *)children
{
    return [[_private->children copy] autorelease];
}

+ (void) registerViewClass:(Class)viewClass representationClass: (Class)representationClass forMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypes] setObject:viewClass forKey:MIMEType];
    [[WebDataSource _repTypes] setObject:representationClass forKey:MIMEType];
}

@end
