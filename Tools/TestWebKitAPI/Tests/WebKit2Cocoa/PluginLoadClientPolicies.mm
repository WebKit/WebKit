/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKContextPrivateMac.h>
#import <WebKit/WKPluginInformation.h>
#import <WebKit/WKProcessPoolPrivate.h>

#if WK_API_ENABLED

TEST(WKProcessPool, resetPluginLoadClientPolicies)
{
    auto processPool = adoptWK([[WKProcessPool alloc] init]);
    WKContextRef wkContext = (WKContextRef)processPool.get();
    
    auto emptyHost = adoptWK(WKStringCreateWithUTF8CString(""));
    auto appleHost = adoptWK(WKStringCreateWithUTF8CString("apple.com"));
    auto flashIdentifier = adoptWK(WKStringCreateWithUTF8CString("com.macromedia.Flash Player.plugin"));
    auto flashVersion = adoptWK(WKStringCreateWithUTF8CString("26.0.0.126"));
    auto silverlightIdentifier = adoptWK(WKStringCreateWithUTF8CString("com.microsoft.SilverlightPlugin"));
    auto silverlightVersion = adoptWK(WKStringCreateWithUTF8CString("5.1.50901.0"));
    WKContextSetPluginLoadClientPolicy(wkContext, kWKPluginLoadClientPolicyAsk, emptyHost.get(), flashIdentifier.get(), flashVersion.get());
    WKContextSetPluginLoadClientPolicy(wkContext, kWKPluginLoadClientPolicyAllowAlways, appleHost.get(), flashIdentifier.get(), flashVersion.get());
    WKContextSetPluginLoadClientPolicy(wkContext, kWKPluginLoadClientPolicyBlock, emptyHost.get(), silverlightIdentifier.get(), silverlightVersion.get());
    WKContextSetPluginLoadClientPolicy(wkContext, kWKPluginLoadClientPolicyAllow, appleHost.get(), silverlightIdentifier.get(), silverlightVersion.get());
    
    NSDictionary *policies = processPool.get()._pluginLoadClientPolicies;
    EXPECT_EQ(2U, policies.count);
    NSDictionary *policiesForEmptyHost = policies[@""];
    EXPECT_EQ(2U, policiesForEmptyHost.count);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForEmptyHost[@"com.macromedia.Flash Player.plugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyAsk, [policiesForEmptyHost[@"com.macromedia.Flash Player.plugin"][@"26.0.0.126"] unsignedIntegerValue]);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForEmptyHost[@"com.microsoft.SilverlightPlugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyBlock, [policiesForEmptyHost[@"com.microsoft.SilverlightPlugin"][@"5.1.50901.0"] unsignedIntegerValue]);
    
    NSDictionary *policiesForAppleHost = policies[@"apple.com"];
    EXPECT_EQ(2U, policiesForAppleHost.count);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForAppleHost[@"com.macromedia.Flash Player.plugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyAllowAlways, [policiesForAppleHost[@"com.macromedia.Flash Player.plugin"][@"26.0.0.126"] unsignedIntegerValue]);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForAppleHost[@"com.microsoft.SilverlightPlugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyAllow, [policiesForAppleHost[@"com.microsoft.SilverlightPlugin"][@"5.1.50901.0"] unsignedIntegerValue]);
    
    NSMutableDictionary *newPolicies = [policies mutableCopy];
    [newPolicies removeObjectForKey:@"apple.com"];
    newPolicies[@"google.com"] = [[NSMutableDictionary alloc] init];
    newPolicies[@"google.com"][@"com.macromedia.Flash Player.plugin"] = [[NSMutableDictionary alloc] init];
    newPolicies[@"google.com"][@"com.macromedia.Flash Player.plugin"][@"26.0.0.126"] = [[NSNumber alloc] initWithUnsignedInt:kWKPluginLoadClientPolicyAllowAlways];
    newPolicies[@"google.com"][@"com.apple.QuickTime.plugin"] = [[NSMutableDictionary alloc] init];
    newPolicies[@"google.com"][@"com.apple.QuickTime.plugin"][@"1.0"] = [[NSNumber alloc] initWithUnsignedInt:kWKPluginLoadClientPolicyBlock];
    newPolicies[@"google.com"][@"com.apple.QuickTime.plugin"][@"1.1"] = [[NSNumber alloc] initWithUnsignedInt:kWKPluginLoadClientPolicyAllow];
    
    [processPool.get() _resetPluginLoadClientPolicies:newPolicies];
    
    policies = processPool.get()._pluginLoadClientPolicies;
    EXPECT_EQ(2U, policies.count);
    policiesForEmptyHost = policies[@""];
    EXPECT_EQ(2U, policiesForEmptyHost.count);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForEmptyHost[@"com.macromedia.Flash Player.plugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyAsk, [policiesForEmptyHost[@"com.macromedia.Flash Player.plugin"][@"26.0.0.126"] unsignedIntegerValue]);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForEmptyHost[@"com.microsoft.SilverlightPlugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyBlock, [policiesForEmptyHost[@"com.microsoft.SilverlightPlugin"][@"5.1.50901.0"] unsignedIntegerValue]);
    
    EXPECT_EQ(true, !policies[@"apple.com"]);
    
    NSDictionary *policiesForGoogleHost = policies[@"google.com"];
    EXPECT_EQ(2U, policiesForGoogleHost.count);
    EXPECT_EQ(1U, ((NSDictionary *)policiesForGoogleHost[@"com.macromedia.Flash Player.plugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyAllowAlways, [policiesForGoogleHost[@"com.macromedia.Flash Player.plugin"][@"26.0.0.126"] unsignedIntegerValue]);
    EXPECT_EQ(2U, ((NSDictionary *)policiesForGoogleHost[@"com.apple.QuickTime.plugin"]).count);
    EXPECT_EQ(kWKPluginLoadClientPolicyBlock, [policiesForGoogleHost[@"com.apple.QuickTime.plugin"][@"1.0"] unsignedIntegerValue]);
    EXPECT_EQ(kWKPluginLoadClientPolicyAllow, [policiesForGoogleHost[@"com.apple.QuickTime.plugin"][@"1.1"] unsignedIntegerValue]);
    
    WKContextClearPluginClientPolicies(wkContext);
    policies = processPool.get()._pluginLoadClientPolicies;
    EXPECT_EQ(0U, policies.count);
}

#endif
#endif
