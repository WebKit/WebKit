/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WebInspectorPreferenceObserver.h"

#import "WebProcessPool.h"

#import <wtf/RetainPtr.h>

@interface WKWebInspectorPreferenceObserver ()
{
@private
    RetainPtr<NSUserDefaults> m_userDefaults;
}
@end

@implementation WKWebInspectorPreferenceObserver

+ (id)sharedInstance
{
    static WKWebInspectorPreferenceObserver *instance = nil;

    if (!instance)
        instance = [[[self class] alloc] init];

    return instance;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    m_userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:@"com.apple.Safari.SandboxBroker"]);
    if (!m_userDefaults) {
        WTFLogAlways("Could not init user defaults instance for domain com.apple.Safari.SandboxBroker.");
        return self;
    }
    [m_userDefaults.get() addObserver:self forKeyPath:@"ShowDevelopMenu" options:NSKeyValueObservingOptionNew context:nil];

    
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    dispatch_async(dispatch_get_main_queue(), ^{
        for (auto* pool : WebKit::WebProcessPool::allProcessPools()) {
            for (size_t i = 0; i < pool->processes().size(); ++i) {
                auto process = pool->processes()[i];
                process->enableRemoteInspectorIfNeeded();
            }
        }
    });
}

@end
