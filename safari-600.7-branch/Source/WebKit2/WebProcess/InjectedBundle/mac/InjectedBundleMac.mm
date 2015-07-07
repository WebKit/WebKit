/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "InjectedBundle.h"

#import "APIData.h"
#import "ObjCObjectGraph.h"
#import "WKBundleAPICast.h"
#import "WKBundleInitialize.h"
#import "WKWebProcessBundleParameters.h"
#import "WKWebProcessPlugInInternal.h"
#import "WebProcessCreationParameters.h"
#import <Foundation/NSBundle.h>
#import <stdio.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

@interface NSBundle (WKAppDetails)
- (CFBundleRef)_cfBundle;
@end

namespace WebKit {

bool InjectedBundle::initialize(const WebProcessCreationParameters& parameters, API::Object* initializationUserData)
{
    if (m_sandboxExtension) {
        if (!m_sandboxExtension->consumePermanently()) {
            WTFLogAlways("InjectedBundle::load failed - Could not consume bundle sandbox extension for [%s].\n", m_path.utf8().data());
            return false;
        }

        m_sandboxExtension = 0;
    }
    
    RetainPtr<CFStringRef> injectedBundlePathStr = m_path.createCFString();
    if (!injectedBundlePathStr) {
        WTFLogAlways("InjectedBundle::load failed - Could not create the path string.\n");
        return false;
    }
    
    RetainPtr<CFURLRef> bundleURL = adoptCF(CFURLCreateWithFileSystemPath(0, injectedBundlePathStr.get(), kCFURLPOSIXPathStyle, false));
    if (!bundleURL) {
        WTFLogAlways("InjectedBundle::load failed - Could not create the url from the path string.\n");
        return false;
    }

    m_platformBundle = [[NSBundle alloc] initWithURL:(NSURL *)bundleURL.get()];
    if (!m_platformBundle) {
        WTFLogAlways("InjectedBundle::load failed - Could not create the bundle.\n");
        return false;
    }
        
    if (![m_platformBundle load]) {
        WTFLogAlways("InjectedBundle::load failed - Could not load the executable from the bundle.\n");
        return false;
    }

    // First check to see if the bundle has a WKBundleInitialize function.
    WKBundleInitializeFunctionPtr initializeFunction = reinterpret_cast<WKBundleInitializeFunctionPtr>(CFBundleGetFunctionPointerForName([m_platformBundle _cfBundle], CFSTR("WKBundleInitialize")));
    if (initializeFunction) {
        initializeFunction(toAPI(this), toAPI(initializationUserData));
        return true;
    }
    
#if WK_API_ENABLED
    if (parameters.bundleParameterData) {
        auto bundleParameterData = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(parameters.bundleParameterData->bytes())) length:parameters.bundleParameterData->size() freeWhenDone:NO]);

        auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:bundleParameterData.get()]);
        [unarchiver setRequiresSecureCoding:YES];

        NSDictionary *dictionary = nil;
        @try {
            dictionary = [unarchiver.get() decodeObjectOfClass:[NSObject class] forKey:@"parameters"];
            ASSERT([dictionary isKindOfClass:[NSDictionary class]]);
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to decode bundle parameters: %@", exception);
        }

        m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:dictionary]);
    }

    // Otherwise, look to see if the bundle has a principal class
    Class principalClass = [m_platformBundle principalClass];
    if (!principalClass) {
        WTFLogAlways("InjectedBundle::load failed - No initialize function or principal class found in the bundle executable.\n");
        return false;
    }

    if (![principalClass conformsToProtocol:@protocol(WKWebProcessPlugIn)]) {
        WTFLogAlways("InjectedBundle::load failed - Principal class does not conform to the WKWebProcessPlugIn protocol.\n");
        return false;
    }

    id <WKWebProcessPlugIn> instance = (id <WKWebProcessPlugIn>)[[principalClass alloc] init];
    if (!instance) {
        WTFLogAlways("InjectedBundle::load failed - Could not initialize an instance of the principal class.\n");
        return false;
    }

    WKWebProcessPlugInController* plugInController = WebKit::wrapper(*this);
    [plugInController _setPrincipalClassInstance:instance];

    if ([instance respondsToSelector:@selector(webProcessPlugIn:initializeWithObject:)]) {
        RetainPtr<id> objCInitializationUserData;
        if (initializationUserData && initializationUserData->type() == API::Object::Type::ObjCObjectGraph)
            objCInitializationUserData = static_cast<ObjCObjectGraph*>(initializationUserData)->rootObject();
        [instance webProcessPlugIn:plugInController initializeWithObject:objCInitializationUserData.get()];
    }

    return true;
#else
    return false;
#endif
}

#if WK_API_ENABLED
WKWebProcessBundleParameters *InjectedBundle::bundleParameters()
{
    // We must not return nil even if no parameters are currently set, in order to allow the client
    // to use KVO.
    if (!m_bundleParameters)
        m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:@{ }]);

    return m_bundleParameters.get();
}
#endif

void InjectedBundle::setBundleParameter(const String& key, const IPC::DataReference& value)
{
#if WK_API_ENABLED
    auto bundleParameterData = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<void*>(static_cast<const void*>(value.data())) length:value.size() freeWhenDone:NO]);

    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:bundleParameterData.get()]);
    [unarchiver setRequiresSecureCoding:YES];

    id parameter = nil;
    @try {
        parameter = [unarchiver decodeObjectOfClass:[NSObject class] forKey:@"parameter"];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode bundle parameter: %@", exception);
    }

    if (!m_bundleParameters && parameter)
        m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:[NSDictionary dictionary]]);

    [m_bundleParameters setParameter:parameter forKey:key];
#endif
}

} // namespace WebKit
