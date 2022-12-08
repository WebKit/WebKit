/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionControllerConfigurationInternal.h"

#import "WebExtensionControllerConfiguration.h"
#import <WebCore/WebCoreObjCExtras.h>

static constexpr NSString *persistentCodingKey = @"persistent";
static constexpr NSString *identifierCodingKey = @"identifier";

@implementation _WKWebExtensionControllerConfiguration

+ (BOOL)supportsSecureCoding
{
    return YES;
}

#if ENABLE(WK_WEB_EXTENSIONS)

+ (instancetype)defaultConfiguration
{
    return WebKit::WebExtensionControllerConfiguration::createDefault()->wrapper();
}

+ (instancetype)nonPersistentConfiguration
{
    return WebKit::WebExtensionControllerConfiguration::createNonPersistent()->wrapper();
}

+ (instancetype)configurationWithIdentifier:(NSUUID *)identifier
{
    NSParameterAssert(identifier);

    return WebKit::WebExtensionControllerConfiguration::create(identifier)->wrapper();
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    NSParameterAssert(coder);

    [coder encodeObject:self.identifier forKey:identifierCodingKey];
    [coder encodeBool:self.persistent forKey:persistentCodingKey];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    NSParameterAssert(coder);

    if (!(self = [super init]))
        return nil;

    using IsPersistent = WebKit::WebExtensionControllerConfiguration::IsPersistent;

    NSUUID *identifier = [coder decodeObjectOfClass:NSUUID.class forKey:identifierCodingKey];
    BOOL persistent = [coder decodeBoolForKey:persistentCodingKey];

    if (identifier)
        API::Object::constructInWrapper<WebKit::WebExtensionControllerConfiguration>(self, identifier);
    else
        API::Object::constructInWrapper<WebKit::WebExtensionControllerConfiguration>(self, persistent ? IsPersistent::Yes : IsPersistent::No);

    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    return _webExtensionControllerConfiguration->copy()->wrapper();
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebExtensionControllerConfiguration.class, self))
        return;

    _webExtensionControllerConfiguration->~WebExtensionControllerConfiguration();
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<_WKWebExtensionControllerConfiguration>(object);
    if (!other)
        return NO;

    return *_webExtensionControllerConfiguration == *other->_webExtensionControllerConfiguration;
}

- (NSString *)debugDescription
{
    return [NSString stringWithFormat:@"<%@: %p; persistent = %@; identifier = %@>", NSStringFromClass(self.class), self, self.persistent ? @"YES" : @"NO", self.identifier];
}

- (NSUUID *)identifier
{
    if (auto identifier = _webExtensionControllerConfiguration->identifier())
        return identifier.value();
    return nil;
}

- (BOOL)isPersistent
{
    return _webExtensionControllerConfiguration->storageIsPersistent();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionControllerConfiguration;
}

- (WebKit::WebExtensionControllerConfiguration&)_webExtensionControllerConfiguration
{
    return *_webExtensionControllerConfiguration;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

+ (instancetype)defaultConfiguration
{
    return nil;
}

+ (instancetype)nonPersistentConfiguration
{
    return nil;
}

+ (instancetype)configurationWithIdentifier:(NSUUID *)identifier
{
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return nil;
}

- (NSUUID *)identifier
{
    return nil;
}

- (BOOL)isPersistent
{
    return NO;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
