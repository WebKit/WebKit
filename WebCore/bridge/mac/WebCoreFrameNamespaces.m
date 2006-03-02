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

#include "config.h"
#import "WebCoreFrameNamespaces.h"

static CFSetCallBacks NonRetainingSetCallbacks = {
0,
NULL,
NULL,
CFCopyDescription,
CFEqual,
CFHash
};

@implementation WebCoreFrameNamespaces

NSMutableDictionary *namespaces = nil;

+(void)addFrame:(WebCoreFrameBridge *)frame toNamespace:(NSString *)name
{
    if (!name)
        return;

    if (!namespaces)
        namespaces = [[NSMutableDictionary alloc] init];

    CFMutableSetRef namespace = (CFMutableSetRef)[namespaces objectForKey:name];

    if (!namespace) {
        namespace = CFSetCreateMutable(NULL, 0, &NonRetainingSetCallbacks);
        [namespaces setObject:(id)namespace forKey:name];
        CFRelease(namespace);
    }
    
    CFSetSetValue(namespace, frame);
}

+(void)removeFrame:(WebCoreFrameBridge *)frame fromNamespace:(NSString *)name
{
    if (!name)
        return;

    CFMutableSetRef namespace = (CFMutableSetRef)[namespaces objectForKey:name];

    if (!namespace)
        return;

    CFSetRemoveValue(namespace, frame);

    if (CFSetGetCount(namespace) == 0)
        [namespaces removeObjectForKey:name];
}

+(NSEnumerator *)framesInNamespace:(NSString *)name;
{
    if (!name)
        return [[[NSEnumerator alloc] init] autorelease];

    CFMutableSetRef namespace = (CFMutableSetRef)[namespaces objectForKey:name];

    if (!namespace)
        return [[[NSEnumerator alloc] init] autorelease];
    
    return [(NSSet *)namespace objectEnumerator];
}

@end
