/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebUnarchivingState.h"

#import "WebArchive.h"
#import "WebAssertions.h"
#import "WebResource.h"
#import "WebNSURLExtras.h"

@implementation WebUnarchivingState

- (id)init
{
    if (!(self = [super init]))
        return nil;
    
    archivedSubframes = [[NSMutableDictionary alloc] init];
    archivedResources = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)addArchive:(WebArchive *)archive
{
    NSEnumerator *enumerator = [[archive subresources] objectEnumerator];
    WebResource *subresource;
    while ((subresource = [enumerator nextObject]) != nil)
        [archivedResources setObject:subresource forKey:[[subresource URL] _web_originalDataAsString]];

    enumerator = [[archive subframeArchives] objectEnumerator];
    WebArchive *subframeArchive;
    while ((subframeArchive = [enumerator nextObject]) != nil) {
        NSString *frameName = [[subframeArchive mainResource] frameName];
        if (frameName)
            [archivedSubframes setObject:subframeArchive forKey:frameName];
    }
}

- (WebResource *)archivedResourceForURL:(NSURL *)URL
{
    return [archivedResources objectForKey:URL];
}

- (WebArchive *)popSubframeArchiveWithFrameName:(NSString *)frameName
{
    ASSERT(frameName != nil);
    
    WebArchive *archive = [[[archivedSubframes objectForKey:frameName] retain] autorelease];
    if (archive != nil)
        [archivedSubframes removeObjectForKey:frameName];

    return archive;
}

@end
