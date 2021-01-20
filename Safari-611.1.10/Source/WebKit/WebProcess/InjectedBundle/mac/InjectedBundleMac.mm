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
#import "ObjCObjectGraph.h"
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

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)

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

static RetainPtr<NSKeyedUnarchiver> createUnarchiver(const unsigned char* bytes, NSUInteger length)
{
    auto data = adoptNS([[NSData alloc] initWithBytesNoCopy:const_cast<unsigned char*>(bytes) length:length freeWhenDone:NO]);
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:data.get() error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    return unarchiver;
}

static RetainPtr<NSKeyedUnarchiver> createUnarchiver(const API::Data& data)
{
    return createUnarchiver(data.bytes(), data.size());
}

static RetainPtr<NSKeyedUnarchiver> createUnarchiver(const IPC::DataReference& data)
{
    return createUnarchiver(data.data(), data.size());
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

bool InjectedBundle::initialize(const WebProcessCreationParameters& parameters, API::Object* initializationUserData)
{
    if (m_sandboxExtension) {
        if (!m_sandboxExtension->consumePermanently()) {
            WTFLogAlways("InjectedBundle::load failed - Could not consume bundle sandbox extension for [%s].\n", m_path.utf8().data());
            return false;
        }

        m_sandboxExtension = nullptr;
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

    m_platformBundle = [[NSBundle alloc] initWithURL:(__bridge NSURL *)bundleURL.get()];
    if (!m_platformBundle) {
        WTFLogAlways("InjectedBundle::load failed - Could not create the bundle.\n");
        return false;
    }

    WKBundleAdditionalClassesForParameterCoderFunctionPtr additionalClassesForParameterCoderFunction = nullptr;
    WKBundleInitializeFunctionPtr initializeFunction = nullptr;
    if (RetainPtr<CFURLRef> executableURL = adoptCF(CFBundleCopyExecutableURL([m_platformBundle _cfBundle]))) {
        static constexpr size_t maxPathSize = 4096;
        char pathToExecutable[maxPathSize];
        if (CFURLGetFileSystemRepresentation(executableURL.get(), true, bitwise_cast<uint8_t*>(pathToExecutable), maxPathSize)) {
            // We don't hold onto this handle anywhere more permanent since we never dlcose.
            if (void* handle = dlopen(pathToExecutable, RTLD_LAZY | RTLD_GLOBAL | RTLD_FIRST)) {
                additionalClassesForParameterCoderFunction = bitwise_cast<WKBundleAdditionalClassesForParameterCoderFunctionPtr>(dlsym(handle, "WKBundleAdditionalClassesForParameterCoder"));
                initializeFunction = bitwise_cast<WKBundleInitializeFunctionPtr>(dlsym(handle, "WKBundleInitialize"));
            }
        }
    }
        
    if (!initializeFunction) {
        if (![m_platformBundle load]) {
            WTFLogAlways("InjectedBundle::load failed - Could not load the executable from the bundle.\n");
            return false;
        }
    }

    if (!initializeFunction)
        initializeFunction = bitwise_cast<WKBundleInitializeFunctionPtr>(CFBundleGetFunctionPointerForName([m_platformBundle _cfBundle], CFSTR("WKBundleInitialize")));

    if (!additionalClassesForParameterCoderFunction)
        additionalClassesForParameterCoderFunction = bitwise_cast<WKBundleAdditionalClassesForParameterCoderFunctionPtr>(CFBundleGetFunctionPointerForName([m_platformBundle _cfBundle], CFSTR("WKBundleAdditionalClassesForParameterCoder")));

    // Update list of valid classes for the parameter coder
    if (additionalClassesForParameterCoderFunction)
        additionalClassesForParameterCoderFunction(toAPI(this), toAPI(initializationUserData));

#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    // Swizzle [NSEvent modiferFlags], since it always returns 0 when the WindowServer is blocked.
    Method method = class_getClassMethod([NSEvent class], @selector(modifierFlags));
    method_setImplementation(method, reinterpret_cast<IMP>(currentModifierFlags));
#endif

    // First check to see if the bundle has a WKBundleInitialize function.
    if (initializeFunction) {
        if (!decodeBundleParameters(parameters.bundleParameterData.get()))
            return false;
        initializeFunction(toAPI(this), toAPI(initializationUserData));
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

    id <WKWebProcessPlugIn> instance = (id <WKWebProcessPlugIn>)[(NSObject *)[principalClass alloc] init];
    if (!instance) {
        WTFLogAlways("InjectedBundle::load failed - Could not initialize an instance of the principal class.\n");
        return false;
    }

    WKWebProcessPlugInController* plugInController = WebKit::wrapper(*this);
    [plugInController _setPrincipalClassInstance:instance];

    if ([instance respondsToSelector:@selector(additionalClassesForParameterCoder)])
        [plugInController extendClassesForParameterCoder:[instance additionalClassesForParameterCoder]];

    if (!decodeBundleParameters(parameters.bundleParameterData.get()))
        return false;

    if ([instance respondsToSelector:@selector(webProcessPlugIn:initializeWithObject:)]) {
        RetainPtr<id> objCInitializationUserData;
        if (initializationUserData && initializationUserData->type() == API::Object::Type::ObjCObjectGraph)
            objCInitializationUserData = static_cast<ObjCObjectGraph*>(initializationUserData)->rootObject();
        [instance webProcessPlugIn:plugInController initializeWithObject:objCInitializationUserData.get()];
    }

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
        m_classesForCoder = [NSSet setWithObjects:[NSArray class], [NSData class], [NSDate class], [NSDictionary class], [NSNull class], [NSNumber class], [NSSet class], [NSString class], [NSTimeZone class], [NSURL class], [NSUUID class], nil];

    return m_classesForCoder.get();
}

void InjectedBundle::setBundleParameter(const String& key, const IPC::DataReference& value)
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

void InjectedBundle::setBundleParameters(const IPC::DataReference& value)
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
