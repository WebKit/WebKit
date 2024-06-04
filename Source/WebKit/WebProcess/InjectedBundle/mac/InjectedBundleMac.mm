/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#import "APIArray.h"
#import "APIData.h"
#import "WKBrowsingContextHandle.h"
#import "WKBundleAPICast.h"
#import "WKBundleInitialize.h"
#import "WKWebProcessBundleParameters.h"
#import "WKWebProcessPlugInInternal.h"
#import "WebProcessCreationParameters.h"
#import <CoreFoundation/CFURL.h>
#import <Foundation/NSBundle.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <dlfcn.h>
#import <objc/runtime.h>
#import <stdio.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

@interface NSBundle (WKAppDetails)
- (CFBundleRef)_cfBundle;
@end

namespace WebKit {
using namespace WebCore;

#if PLATFORM(MAC)
static NSEventModifierFlags currentModifierFlags(id self, SEL _cmd)
{
    auto currentModifiers = PlatformKeyboardEvent::currentStateOfModifierKeys();
    NSEventModifierFlags modifiers = 0;
    
    if (currentModifiers.contains(PlatformEvent::Modifier::ShiftKey))
        modifiers |= NSEventModifierFlagShift;
    if (currentModifiers.contains(PlatformEvent::Modifier::ControlKey))
        modifiers |= NSEventModifierFlagControl;
    if (currentModifiers.contains(PlatformEvent::Modifier::AltKey))
        modifiers |= NSEventModifierFlagOption;
    if (currentModifiers.contains(PlatformEvent::Modifier::MetaKey))
        modifiers |= NSEventModifierFlagCommand;
    if (currentModifiers.contains(PlatformEvent::Modifier::CapsLockKey))
        modifiers |= NSEventModifierFlagCapsLock;
    
    return modifiers;
}
#endif

static RetainPtr<NSKeyedUnarchiver> createUnarchiver(std::span<const uint8_t> span)
{
    RetainPtr data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<uint8_t*>(span.data()) length:span.size() freeWhenDone:NO]);
    RetainPtr unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:data.get() error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    return unarchiver;
}

static RetainPtr<NSKeyedUnarchiver> createUnarchiver(const API::Data& data)
{
    return createUnarchiver(data.span());
}

bool InjectedBundle::decodeBundleParameters(API::Data* bundleParameterDataPtr)
{
    if (!bundleParameterDataPtr)
        return true;

    auto unarchiver = createUnarchiver(*bundleParameterDataPtr);

    NSDictionary *dictionary = nil;
    @try {
        dictionary = [unarchiver.get() decodeObjectOfClasses:classesForCoder() forKey:@"parameters"];
        if (![dictionary isKindOfClass:[NSDictionary class]]) {
            WTFLogAlways("InjectedBundle::decodeBundleParameters failed - Resulting object was not an NSDictionary.\n");
            return false;
        }
    } @catch (NSException *exception) {
        LOG_ERROR("InjectedBundle::decodeBundleParameters failed to decode bundle parameters: %@." , exception);
        return false;
    }
    
    ASSERT(!m_bundleParameters || m_bundleParameters.get());
    m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:dictionary]);
    return true;
}

bool InjectedBundle::initialize(const WebProcessCreationParameters& parameters, RefPtr<API::Object>&& initializationUserData)
{
    if (auto sandboxExtension = std::exchange(m_sandboxExtension, nullptr)) {
        if (!sandboxExtension->consumePermanently()) {
            WTFLogAlways("InjectedBundle::load failed - Could not consume bundle sandbox extension for [%s].\n", m_path.utf8().data());
            return false;
        }
    }

    m_platformBundle = [[NSBundle alloc] initWithPath:m_path];
    if (!m_platformBundle) {
        WTFLogAlways("InjectedBundle::load failed - Could not create the bundle.\n");
        return false;
    }

    WKBundleAdditionalClassesForParameterCoderFunctionPtr additionalClassesForParameterCoderFunction = nullptr;
    WKBundleInitializeFunctionPtr initializeFunction = nullptr;
    if (NSString *executablePath = m_platformBundle.executablePath) {
        if (dlopen_preflight(executablePath.fileSystemRepresentation)) {
            // We don't hold onto this handle anywhere more permanent since we never dlclose.
            if (void* handle = dlopen(executablePath.fileSystemRepresentation, RTLD_LAZY | RTLD_GLOBAL | RTLD_FIRST)) {
                additionalClassesForParameterCoderFunction = bitwise_cast<WKBundleAdditionalClassesForParameterCoderFunctionPtr>(dlsym(handle, "WKBundleAdditionalClassesForParameterCoder"));
                initializeFunction = bitwise_cast<WKBundleInitializeFunctionPtr>(dlsym(handle, "WKBundleInitialize"));
            }
        }
    }
        
    if (!initializeFunction) {
        NSError *error;
        if (![m_platformBundle preflightAndReturnError:&error]) {
            NSLog(@"InjectedBundle::load failed - preflightAndReturnError failed, error: %@", error);
            return false;
        }
        if (![m_platformBundle loadAndReturnError:&error]) {
            NSLog(@"InjectedBundle::load failed - loadAndReturnError failed, error: %@", error);
            return false;
        }
        initializeFunction = bitwise_cast<WKBundleInitializeFunctionPtr>(CFBundleGetFunctionPointerForName([m_platformBundle _cfBundle], CFSTR("WKBundleInitialize")));
    }

    if (!additionalClassesForParameterCoderFunction)
        additionalClassesForParameterCoderFunction = bitwise_cast<WKBundleAdditionalClassesForParameterCoderFunctionPtr>(CFBundleGetFunctionPointerForName([m_platformBundle _cfBundle], CFSTR("WKBundleAdditionalClassesForParameterCoder")));

    // Update list of valid classes for the parameter coder
    if (additionalClassesForParameterCoderFunction)
        additionalClassesForParameterCoderFunction(toAPI(this), toAPI(initializationUserData.get()));

#if PLATFORM(MAC)
    // Swizzle [NSEvent modiferFlags], since it always returns 0 when the WindowServer is blocked.
    Method method = class_getClassMethod([NSEvent class], @selector(modifierFlags));
    method_setImplementation(method, reinterpret_cast<IMP>(currentModifierFlags));
#endif

    // First check to see if the bundle has a WKBundleInitialize function.
    if (initializeFunction) {
        if (!decodeBundleParameters(parameters.bundleParameterData.get()))
            return false;
        initializeFunction(toAPI(this), toAPI(initializationUserData.get()));
        return true;
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

    auto instance = adoptNS((id <WKWebProcessPlugIn>)[(NSObject *)[principalClass alloc] init]);
    if (!instance) {
        WTFLogAlways("InjectedBundle::load failed - Could not initialize an instance of the principal class.\n");
        return false;
    }

    WKWebProcessPlugInController* plugInController = WebKit::wrapper(*this);
    [plugInController _setPrincipalClassInstance:instance.get()];

    if ([instance respondsToSelector:@selector(additionalClassesForParameterCoder)])
        [plugInController extendClassesForParameterCoder:[instance additionalClassesForParameterCoder]];

    if (!decodeBundleParameters(parameters.bundleParameterData.get()))
        return false;

    if ([instance respondsToSelector:@selector(webProcessPlugIn:initializeWithObject:)])
        [instance webProcessPlugIn:plugInController initializeWithObject:nil];

    return true;
}

WKWebProcessBundleParameters *InjectedBundle::bundleParameters()
{
    // We must not return nil even if no parameters are currently set, in order to allow the client
    // to use KVO.
    if (!m_bundleParameters)
        m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:@{ }]);

    return m_bundleParameters.get();
}

void InjectedBundle::extendClassesForParameterCoder(API::Array& classes)
{
    size_t size = classes.size();

    auto mutableSet = adoptNS([classesForCoder() mutableCopy]);

    for (size_t i = 0; i < size; ++i) {
        API::String* classNameString = classes.at<API::String>(i);
        if (!classNameString) {
            WTFLogAlways("InjectedBundle::extendClassesForParameterCoder - No class provided as argument %d.\n", i);
            break;
        }
    
        CString className = classNameString->string().utf8();
        Class objectClass = objc_lookUpClass(className.data());
        if (!objectClass) {
            WTFLogAlways("InjectedBundle::extendClassesForParameterCoder - Class %s is not a valid Objective C class.\n", className.data());
            break;
        }

        [mutableSet.get() addObject:objectClass];
    }

    m_classesForCoder = mutableSet;
}

NSSet* InjectedBundle::classesForCoder()
{
    if (!m_classesForCoder)
        m_classesForCoder = [NSSet setWithObjects:[NSArray class], [NSData class], [NSDate class], [NSDictionary class], [NSNull class], [NSNumber class], [NSSet class], [NSString class], [NSTimeZone class], [NSURL class], [NSUUID class], [WKBrowsingContextHandle class], nil];

    return m_classesForCoder.get();
}

void InjectedBundle::setBundleParameter(const String& key, std::span<const uint8_t> value)
{
    id parameter = nil;
    auto unarchiver = createUnarchiver(value);
    @try {
        parameter = [unarchiver decodeObjectOfClasses:classesForCoder() forKey:@"parameter"];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode bundle parameter: %@", exception);
        return;
    }

    if (!m_bundleParameters && parameter)
        m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:@{ }]);

    [m_bundleParameters setParameter:parameter forKey:key];
}

void InjectedBundle::setBundleParameters(std::span<const uint8_t> value)
{
    NSDictionary *parameters = nil;
    auto unarchiver = createUnarchiver(value);
    @try {
        parameters = [unarchiver decodeObjectOfClasses:classesForCoder() forKey:@"parameters"];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode bundle parameter: %@", exception);
    }

    if (!parameters)
        return;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION([parameters isKindOfClass:[NSDictionary class]]);

    if (!m_bundleParameters) {
        m_bundleParameters = adoptNS([[WKWebProcessBundleParameters alloc] initWithDictionary:parameters]);
        return;
    }

    [m_bundleParameters setParametersForKeyWithDictionary:parameters];
}

} // namespace WebKit
