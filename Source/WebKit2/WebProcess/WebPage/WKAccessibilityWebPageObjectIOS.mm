/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKAccessibilityWebPageObjectIOS.h"

#if PLATFORM(IOS)

#import "WebFrame.h"
#import "WebPage.h"
#import <WebCore/FrameView.h>

/* 
 The implementation of this class will be augmented by an accesibility bundle that is loaded only when accessibility is requested to be enabled.
 */

@implementation WKAccessibilityWebPageObject

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_accessibilityCategoryInstalled:) name:@"AccessibilityCategoryInstalled" object:nil];

    return self;
}

- (void)_accessibilityCategoryInstalled:(id)notification
{
    // Accessibility bundle will override this method so that it knows when to initialize the accessibility runtime within the WebProcess.
}

- (double)pageScale
{
    return m_page->pageScaleFactor();
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    self.remoteTokenData = nil;
    [super dealloc];
}

@end

#endif // PLATFORM(IOS)

