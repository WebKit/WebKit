/*	
      WebControllerSets.m
      Copyright 2002, Apple Computer, Inc.
*/

#import <WebKit/WebControllerSets.h>

static CFSetCallBacks NonRetainingSetCallbacks = {
0,
NULL,
NULL,
CFCopyDescription,
CFEqual,
CFHash
};

@implementation WebViewSets

NSMutableDictionary *sets = nil;

+(void)addWebView:(WebView *)webView toSetNamed: (NSString *)name
{
    if (sets == nil) {
	sets = [[NSMutableDictionary alloc] init];
    }

    CFMutableSetRef set = (CFMutableSetRef)[sets objectForKey:name];

    if (set == NULL) {
	set = CFSetCreateMutable(NULL, 0, &NonRetainingSetCallbacks);
	[sets setObject:(id)set forKey:name];
	CFRelease(set);
    }

    
    CFSetSetValue(set, webView);
}

+(void)removeWebView:(WebView *)webView fromSetNamed: (NSString *)name
{
    CFMutableSetRef set = (CFMutableSetRef)[sets objectForKey:name];

    if (set == NULL) {
	return;
    }

    CFSetRemoveValue(set, webView);

    if (CFSetGetCount(set) == 0) {
	[sets removeObjectForKey:name];
    }
}


+(NSEnumerator *)webViewsInSetNamed:(NSString *)name;
{
    CFMutableSetRef set = (CFMutableSetRef)[sets objectForKey:name];

    if (set == NULL) {
	return [[[NSEnumerator alloc] init] autorelease];
    }
    
    return [(NSSet *)set objectEnumerator];
}

+ (void)makeWebViewsPerformSelector:(SEL)selector
{
    NSEnumerator *setEnumerator = [sets objectEnumerator];
    NSMutableSet *set;
    while ((set = [setEnumerator nextObject]) != nil) {
        [set makeObjectsPerformSelector:selector];
    }
}

@end



