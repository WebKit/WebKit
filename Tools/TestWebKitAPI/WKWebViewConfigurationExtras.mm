/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#import "WKWebViewConfigurationExtras.h"

#import "PlatformUtilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

@implementation WKWebViewConfiguration (TestWebKitAPIExtras)

+ (instancetype)_test_configurationWithTestPlugInClassName:(NSString *)className
{
    return [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:className configureJSCForTesting:NO];
}

+ (instancetype)_test_configurationWithTestPlugInClassName:(NSString *)className configureJSCForTesting:(BOOL)value
{
    return [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:className configureJSCForTesting:value andCustomParameterClasses:nil];
}

+ (instancetype)_test_configurationWithTestPlugInClassName:(NSString *)className configureJSCForTesting:(BOOL)value andCustomParameterClasses:(NSSet<Class> *)parameterClasses
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setInjectedBundleURL:[[NSBundle mainBundle] URLForResource:@"TestWebKitAPI" withExtension:@"wkbundle"]];
    [processPoolConfiguration setConfigureJSCForTesting:value];
    [processPoolConfiguration setCustomClassesForParameterCoder: parameterClasses];
    
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    [processPool _setObject:className forBundleParameter:TestWebKitAPI::Util::TestPlugInClassNameParameter];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    return webViewConfiguration.autorelease();
}

@end
