/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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



