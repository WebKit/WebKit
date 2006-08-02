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

#import <WebKit/WebFrameLoader.h>

#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebDataSourceInternal.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebMainResourceLoader.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebViewInternal.h>

@implementation WebFrameLoader

- (id)initWithWebFrame:(WebFrame *)wf
{
    self = [super init];
    if (self) {
        webFrame = wf;
        state = WebFrameStateCommittedPage;
    }
    return self;    
}

- (void)dealloc
{
    // FIXME: should these even exist?
    [mainResourceLoader release];
    [subresourceLoaders release];
    [plugInStreamLoaders release];
    [iconLoader release];
    [dataSource release];
    [provisionalDataSource release];
    
    [super dealloc];
}

- (BOOL)hasIconLoader
{
    return iconLoader != nil;
}

- (void)loadIconWithRequest:(NSURLRequest *)request
{
    ASSERT(!iconLoader);
    iconLoader = [[WebIconLoader alloc] initWithRequest:request];
    [iconLoader setDataSource:dataSource];
    [iconLoader loadWithRequest:request];
}

- (void)stopLoadingIcon
{
    [iconLoader stopLoading];
}

- (void)addPlugInStreamLoader:(WebLoader *)loader
{
    if (!plugInStreamLoaders)
        plugInStreamLoaders = [[NSMutableArray alloc] init];
    [plugInStreamLoaders addObject:loader];
}

- (void)removePlugInStreamLoader:(WebLoader *)loader
{
    [plugInStreamLoaders removeObject:loader];
}    

- (void)setDefersCallbacks:(BOOL)defers
{
    [mainResourceLoader setDefersCallbacks:defers];
    
    NSEnumerator *e = [subresourceLoaders objectEnumerator];
    WebLoader *loader;
    while ((loader = [e nextObject]))
        [loader setDefersCallbacks:defers];
    
    e = [plugInStreamLoaders objectEnumerator];
    while ((loader = [e nextObject]))
        [loader setDefersCallbacks:defers];
}

- (void)stopLoadingPlugIns
{
    [plugInStreamLoaders makeObjectsPerformSelector:@selector(cancel)];
    [plugInStreamLoaders removeAllObjects];   
}

- (BOOL)isLoadingMainResource
{
    return mainResourceLoader != nil;
}

- (BOOL)isLoadingSubresources
{
    return [subresourceLoaders count];
}

- (BOOL)isLoading
{
    return [self isLoadingMainResource] || [self isLoadingSubresources];
}

- (void)stopLoadingSubresources
{
    NSArray *loaders = [subresourceLoaders copy];
    [loaders makeObjectsPerformSelector:@selector(cancel)];
    [loaders release];
    [subresourceLoaders removeAllObjects];
}

- (void)addSubresourceLoader:(WebLoader *)loader
{
    if (subresourceLoaders == nil)
        subresourceLoaders = [[NSMutableArray alloc] init];
    [subresourceLoaders addObject:loader];
}

- (void)removeSubresourceLoader:(WebLoader *)loader
{
    [subresourceLoaders removeObject:loader];
}

- (NSData *)mainResourceData
{
    return [mainResourceLoader resourceData];
}

- (void)releaseMainResourceLoader
{
    [mainResourceLoader release];
    mainResourceLoader = nil;
}

- (void)cancelMainResourceLoad
{
    [mainResourceLoader cancel];
}

- (BOOL)startLoadingMainResourceWithRequest:(NSMutableURLRequest *)request identifier:(id)identifier
{
    mainResourceLoader = [[WebMainResourceLoader alloc] initWithDataSource:provisionalDataSource];
    
    [mainResourceLoader setIdentifier:identifier];
    [[provisionalDataSource webFrame] _addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:NO];
    if (![mainResourceLoader loadWithRequest:request]) {
        // FIXME: if this should really be caught, we should just ASSERT this doesn't happen;
        // should it be caught by other parts of WebKit or other parts of the app?
        LOG_ERROR("could not create WebResourceHandle for URL %@ -- should be caught by policy handler level", [request URL]);
        [mainResourceLoader release];
        mainResourceLoader = nil;
        return NO;
    }
    
    return YES;
}

- (void)stopLoadingWithError:(NSError *)error
{
    [mainResourceLoader cancelWithError:error];
}

- (WebDataSource *)dataSource
{
    return dataSource; 
}

- (void)_setDataSource:(WebDataSource *)ds
{
    if (ds == nil && dataSource == nil)
        return;
    
    ASSERT(ds != dataSource);
    
    [webFrame _prepareForDataSourceReplacement];
    [dataSource _setWebFrame:nil];
    
    [ds retain];
    [dataSource release];
    dataSource = ds;

    [ds _setWebFrame:webFrame];
}

- (void)clearDataSource
{
    [self _setDataSource:nil];
}

- (WebDataSource *)provisionalDataSource 
{
    return provisionalDataSource; 
}

- (void)_setProvisionalDataSource: (WebDataSource *)d
{
    ASSERT(!d || !provisionalDataSource);

    if (provisionalDataSource != dataSource)
        [provisionalDataSource _setWebFrame:nil];

    [d retain];
    [provisionalDataSource release];
    provisionalDataSource = d;

    [d _setWebFrame:webFrame];
}

- (void)_clearProvisionalDataSource
{
    [self _setProvisionalDataSource:nil];
}

- (WebFrameState)state
{
    return state;
}

#ifndef NDEBUG
static const char * const stateNames[] = {
    "WebFrameStateProvisional",
    "WebFrameStateCommittedPage",
    "WebFrameStateComplete"
};
#endif

static CFAbsoluteTime _timeOfLastCompletedLoad;

+ (CFAbsoluteTime)timeOfLastCompletedLoad
{
    return _timeOfLastCompletedLoad;
}

- (void)_setState:(WebFrameState)newState
{
    LOG(Loading, "%@:  transition from %s to %s", [webFrame name], stateNames[state], stateNames[newState]);
    if ([webFrame webView])
        LOG(Timing, "%@:  transition from %s to %s, %f seconds since start of document load", [webFrame name], stateNames[state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[webFrame webView] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && webFrame == [[webFrame webView] mainFrame])
        LOG(DocumentLoad, "completed %@ (%f seconds)", [[[self dataSource] request] URL], CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
    
    state = newState;
    
    if (state == WebFrameStateProvisional)
        [webFrame _provisionalLoadStarted];
    else if (state == WebFrameStateComplete) {
        [webFrame _frameLoadCompleted];
        _timeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();
        [dataSource _stopRecordingResponses];
    }
}

- (void)clearProvisionalLoad
{
    [self _setProvisionalDataSource:nil];
    [[webFrame webView] _progressCompleted:webFrame];
    [self _setState:WebFrameStateComplete];
}

- (void)markLoadComplete
{
    [self _setState:WebFrameStateComplete];
}

- (void)commitProvisionalLoad
{
    [self stopLoadingSubresources];
    [self stopLoadingPlugIns];

    [self _setDataSource:provisionalDataSource];
    [self _setProvisionalDataSource:nil];
    [self _setState:WebFrameStateCommittedPage];
}

- (void)stopLoading
{
    [provisionalDataSource _stopLoading];
    [dataSource _stopLoading];
    [self _clearProvisionalDataSource];
}

// FIXME: poor method name; also why is this not part of startProvisionalLoad:?
- (void)startLoading
{
    [provisionalDataSource _startLoading];
}

- (void)startProvisionalLoad:(WebDataSource *)ds
{
    [self _setProvisionalDataSource:ds];
    [self _setState:WebFrameStateProvisional];
}

- (void)setupForReplace
{
    [self _setState:WebFrameStateProvisional];
    WebDataSource *old = provisionalDataSource;
    provisionalDataSource = dataSource;
    dataSource = nil;
    [old release];
    
    [webFrame _detachChildren];
}

@end
