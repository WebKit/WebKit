/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#import "PolicyDelegate.h"

#import "DumpRenderTree.h"
#import "LayoutTestController.h"
#import <WebKit/WebPolicyDelegate.h>

@implementation PolicyDelegate

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation
                                                           request:(NSURLRequest *)request
                                                             frame:(WebFrame *)frame
                                                  decisionListener:(id<WebPolicyDecisionListener>)listener
{
    WebNavigationType navType = (WebNavigationType)[[actionInformation objectForKey:WebActionNavigationTypeKey] intValue];

    const char* typeDescription;
    switch (navType) {
        case WebNavigationTypeLinkClicked:
            typeDescription = "link clicked";
            break;
        case WebNavigationTypeFormSubmitted:
            typeDescription = "form submitted";
            break;
        case WebNavigationTypeBackForward:
            typeDescription = "back/forward";
            break;
        case WebNavigationTypeReload:
            typeDescription = "reload";
            break;
        case WebNavigationTypeFormResubmitted:
            typeDescription = "form resubmitted";
            break;
        case WebNavigationTypeOther:
            typeDescription = "other";
            break;
        default:
            typeDescription = "illegal value";
    }
    
    printf("Policy delegate: attempt to load %s with navigation type '%s'\n", [[[request URL] absoluteString] UTF8String], typeDescription);
    
    if (permissiveDelegate)
        [listener use];
    else
        [listener ignore];

    if (controllerToNotifyDone) {
        controllerToNotifyDone->notifyDone();
        controllerToNotifyDone = 0;
    }
}

- (void)setPermissive:(BOOL)permissive
{
    permissiveDelegate = permissive;
}

- (void)setControllerToNotifyDone:(LayoutTestController*)controller
{
    controllerToNotifyDone = controller;
}

@end
