/*
 * Copyright (C) 2019 Microsoft Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "_WKBrowserInspector.h"

#include "BrowserInspectorPipe.h"
#include "InspectorPlaywrightAgentClientMac.h"
#include "PageClientImplMac.h"
#include "WebKit2Initialize.h"

#import "WKWebView.h"

using namespace WebKit;

@implementation _WKBrowserInspector

+ (void)initializeRemoteInspectorPipe:(id<_WKBrowserInspectorDelegate>)delegate headless:(BOOL)headless
{
#if ENABLE(REMOTE_INSPECTOR)
    InitializeWebKit2();
    PageClientImpl::setHeadless(headless);
    initializeBrowserInspectorPipe(makeUnique<InspectorPlaywrightAgentClientMac>(delegate));
#endif
}

@end

@implementation _WKBrowserContext
- (void)dealloc
{
    [_dataStore release];
    [_processPool release];
    _dataStore = nil;
    _processPool = nil;
    [super dealloc];
}
@end
