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

@implementation WebControllerSets

NSMutableDictionary *sets = nil;

+(void)addController:(WebView *)controller toSetNamed: (NSString *)name
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

    
    CFSetSetValue(set, controller);
}

+(void)removeController:(WebView *)controller fromSetNamed: (NSString *)name
{
    CFMutableSetRef set = (CFMutableSetRef)[sets objectForKey:name];

    if (set == NULL) {
	return;
    }

    CFSetRemoveValue(set, controller);

    if (CFSetGetCount(set) == 0) {
	[sets removeObjectForKey:name];
    }
}


+(NSEnumerator *)controllersInSetNamed:(NSString *)name;
{
    CFMutableSetRef set = (CFMutableSetRef)[sets objectForKey:name];

    if (set == NULL) {
	return [[[NSEnumerator alloc] init] autorelease];
    }
    
    return [(NSSet *)set objectEnumerator];
}

@end



