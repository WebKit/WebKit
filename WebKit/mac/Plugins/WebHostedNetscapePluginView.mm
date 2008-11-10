/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#if USE(PLUGIN_HOST_PROCESS)

#import "WebHostedNetscapePluginView.h"

#import "WebKitSystemInterface.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/Assertions.h>

@implementation WebHostedNetscapePluginView

+ (void)initialize
{
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
    WKSendUserChangeNotifications();
}

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
         DOMElement:(DOMElement *)element
{
    self = [super initWithFrame:frame pluginPackage:pluginPackage URL:URL baseURL:baseURL MIMEType:MIME attributeKeys:keys attributeValues:values loadManually:loadManually DOMElement:element];
    if (!self)
        return nil;
    
    return self;
}    

- (void)handleMouseMoved:(NSEvent *)event
{
    // FIXME: Implement.
}

- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values
{
    ASSERT(!_attributeKeys && !_attributeValues);
    
    _attributeKeys = [keys copy];
    _attributeValues = [values copy];
}    

@end

#endif
