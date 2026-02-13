/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKContentWorldConfigurationInternal.h"

#include "_WKContentWorldConfiguration.h"

@implementation WKContentWorldConfiguration

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::ContentWorldConfiguration>(self);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKContentWorldConfiguration.class, self))
        return;

    self._protectedWorldConfiguration->API::ContentWorldConfiguration::~ContentWorldConfiguration();

    [super dealloc];
}

- (Ref<API::ContentWorldConfiguration>)_protectedWorldConfiguration
{
    return *_worldConfiguration;
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    return wrapper(self._protectedWorldConfiguration->copy()).autorelease();
}

#pragma mark NSSecureCoding protocol implementation

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    Ref<API::ContentWorldConfiguration> configuration = self._protectedWorldConfiguration;

    [coder encodeObject:nsStringNilIfEmpty(configuration->name()) forKey:@"name"];
    [coder encodeBool:configuration->allowAccessToClosedShadowRoots() forKey:@"allowAccessToClosedShadowRoots"];
    [coder encodeBool:configuration->allowAutofill() forKey:@"allowAutofill"];
    [coder encodeBool:configuration->allowElementUserInfo() forKey:@"allowElementUserInfo"];
    [coder encodeBool:configuration->disableLegacyBuiltinOverrides() forKey:@"disableLegacyBuiltinOverrides"];
    [coder encodeBool:configuration->allowJSHandleCreation() forKey:@"allowJSHandleCreation"];
    [coder encodeBool:configuration->allowNodeSerialization() forKey:@"allowNodeSerialization"];
    [coder encodeBool:configuration->isInspectable() forKey:@"inspectable"];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    Ref<API::ContentWorldConfiguration> configuration = self._protectedWorldConfiguration;

    configuration->setName([coder decodeObjectOfClass:[NSString class] forKey:@"name"]);
    configuration->setAllowAccessToClosedShadowRoots([coder decodeBoolForKey:@"allowAccessToClosedShadowRoots"]);
    configuration->setAllowAutofill([coder decodeBoolForKey:@"allowAutofill"]);
    configuration->setAllowElementUserInfo([coder decodeBoolForKey:@"allowElementUserInfo"]);
    configuration->setDisableLegacyBuiltinOverrides([coder decodeBoolForKey:@"disableLegacyBuiltinOverrides"]);
    configuration->setAllowJSHandleCreation([coder decodeBoolForKey:@"allowJSHandleCreation"]);
    configuration->setAllowNodeSerialization([coder decodeBoolForKey:@"allowNodeSerialization"]);
    configuration->setInspectable([coder decodeBoolForKey:@"inspectable"]);

    return self;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_worldConfiguration;
}

- (BOOL)openClosedShadowRootsEnabled
{
    return self._protectedWorldConfiguration->allowAccessToClosedShadowRoots();
}

- (void)setOpenClosedShadowRootsEnabled:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowAccessToClosedShadowRoots(allow);
}

- (BOOL)autofillScriptingEnabled
{
    return self._protectedWorldConfiguration->allowAutofill();
}

- (void)setAutofillScriptingEnabled:(BOOL)enabled
{
    self._protectedWorldConfiguration->setAllowAutofill(enabled);
}

- (BOOL)elementUserInfoEnabled
{
    return self._protectedWorldConfiguration->allowElementUserInfo();
}

- (void)setElementUserInfoEnabled:(BOOL)enabled
{
    self._protectedWorldConfiguration->setAllowElementUserInfo(enabled);
}

- (BOOL)legacyBuiltinOverridesEnabled
{
    return !self._protectedWorldConfiguration->disableLegacyBuiltinOverrides();
}

- (void)setLegacyBuiltinOverridesEnabled:(BOOL)enabled
{
    self._protectedWorldConfiguration->setDisableLegacyBuiltinOverrides(!enabled);
}

- (BOOL)jsHandleCreationEnabled
{
    return self._protectedWorldConfiguration->allowJSHandleCreation();
}

- (void)setJSHandleCreationEnabled:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowJSHandleCreation(allow);
}

- (BOOL)isInspectable
{
    return self._protectedWorldConfiguration->isInspectable();
}

- (void)setInspectable:(BOOL)inspectable
{
    self._protectedWorldConfiguration->setInspectable(inspectable);
}

- (BOOL)nodeSerializationEnabled
{
    return self._protectedWorldConfiguration->allowNodeSerialization();
}

- (void)setNodeSerializationEnabled:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowNodeSerialization(allow);
}

@end

@implementation _WKContentWorldConfiguration

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    return self;
}

- (NSString *)name
{
    return self._protectedWorldConfiguration->name().createNSString().autorelease();
}

- (void)setName:(NSString *)name
{
    self._protectedWorldConfiguration->setName(name);
}

- (BOOL)allowAccessToClosedShadowRoots
{
    return self._protectedWorldConfiguration->allowAccessToClosedShadowRoots();
}

- (void)setAllowAccessToClosedShadowRoots:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowAccessToClosedShadowRoots(allow);
}

- (BOOL)allowAutofill
{
    return self._protectedWorldConfiguration->allowAutofill();
}

- (void)setAllowAutofill:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowAutofill(allow);
}

- (BOOL)allowElementUserInfo
{
    return self._protectedWorldConfiguration->allowElementUserInfo();
}

- (void)setAllowElementUserInfo:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowElementUserInfo(allow);
}

- (BOOL)disableLegacyBuiltinOverrides
{
    return self._protectedWorldConfiguration->disableLegacyBuiltinOverrides();
}

- (void)setDisableLegacyBuiltinOverrides:(BOOL)disable
{
    self._protectedWorldConfiguration->setDisableLegacyBuiltinOverrides(disable);
}

- (BOOL)allowJSHandleCreation
{
    return self._protectedWorldConfiguration->allowJSHandleCreation();
}

- (void)setAllowJSHandleCreation:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowJSHandleCreation(allow);
}

- (BOOL)allowNodeSerialization
{
    return [self nodeSerializationEnabled];
}

- (void)setAllowNodeSerialization:(BOOL)allow
{
    [self setNodeSerializationEnabled:allow];
}

@end
