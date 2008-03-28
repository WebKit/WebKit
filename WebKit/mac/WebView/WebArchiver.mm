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

#import "WebArchiver.h"

#import "DOMNodeInternal.h"
#import "WebArchive.h"
#import "WebArchiveInternal.h"
#import "WebDOMOperationsPrivate.h"
#import "WebDataSource.h"
#import "WebDocument.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebResource.h"
#import <JavaScriptCore/Assertions.h>
#import <WebCore/Frame.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/SelectionController.h>
#import <WebKit/DOM.h>

using namespace WebCore;

@implementation WebArchiver

+ (WebArchive *)archiveFrame:(WebFrame *)frame;
{
    return [[[WebArchive alloc] _initWithCoreLegacyWebArchive:LegacyWebArchive::create(core(frame))] autorelease];
}

+ (WebArchive *)_archiveWithMarkupString:(NSString *)markupString fromFrame:(WebFrame *)frame nodes:(NSArray *)nodes
{ 
    Vector<Node*> coreNodes;
    unsigned count = [nodes count];
    coreNodes.reserveCapacity(count);
    
    for (unsigned i = 0; i < count; ++i)
        coreNodes.append([[nodes objectAtIndex:i] _node]);
        
    return [[[WebArchive alloc] _initWithCoreLegacyWebArchive:LegacyWebArchive::create(markupString, core(frame), coreNodes)] autorelease];
}

+ (WebArchive *)archiveRange:(DOMRange *)range
{
    WebFrame *frame = [[[range startContainer] ownerDocument] webFrame];
    NSArray *nodes;
    NSString *markupString = [frame _markupStringFromRange:range nodes:&nodes];
    return [self _archiveWithMarkupString:markupString fromFrame:frame nodes:nodes];
}

+ (WebArchive *)archiveNode:(DOMNode *)node
{
    WebFrame *frame = [[node ownerDocument] webFrame];
    NSArray *nodes;
    NSString *markupString = [frame _markupStringFromNode:node nodes:&nodes];
    return [self _archiveWithMarkupString:markupString fromFrame:frame nodes:nodes];
}

+ (WebArchive *)archiveSelectionInFrame:(WebFrame *)frame
{
    Frame* coreFrame = core(frame);
    if (!coreFrame)
        return nil;

    NSArray *nodes;
    NSString *markupString = [frame _markupStringFromRange:kit(coreFrame->selectionController()->toRange().get()) nodes:&nodes];
    WebArchive *archive = [self _archiveWithMarkupString:markupString fromFrame:frame nodes:nodes];

    if (coreFrame->isFrameSet()) {
        // Wrap the frameset document in an iframe so it can be pasted into
        // another document (which will have a body or frameset of its own). 

        NSString *iframeMarkup = [[NSString alloc] initWithFormat:@"<iframe frameborder=\"no\" marginwidth=\"0\" marginheight=\"0\" width=\"98%%\" height=\"98%%\" src=\"%@\"></iframe>", [[[frame _dataSource] response] URL]];
        WebResource *iframeResource = [[WebResource alloc] initWithData:[iframeMarkup dataUsingEncoding:NSUTF8StringEncoding]
                                                                  URL:blankURL()
                                                             MIMEType:@"text/html"
                                                     textEncodingName:@"UTF-8"
                                                            frameName:nil];
        
        NSArray *subframeArchives = [NSArray arrayWithObject:archive];
        archive = [[[WebArchive alloc] initWithMainResource:iframeResource subresources:nil subframeArchives:subframeArchives] autorelease];
        
        [iframeResource release];
        [iframeMarkup release];
    }

    return archive;
}

@end
 
