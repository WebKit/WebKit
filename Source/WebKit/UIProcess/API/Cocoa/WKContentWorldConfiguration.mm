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

    protect(*_worldConfiguration)->API::ContentWorldConfiguration::~ContentWorldConfiguration();

    [super dealloc];
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    return wrapper(protect(*_worldConfiguration)->copy()).autorelease();
}

#pragma mark NSSecureCoding protocol implementation

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    Ref<API::ContentWorldConfiguration> configuration = *_worldConfiguration;

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

    Ref<API::ContentWorldConfiguration> configuration = *_worldConfiguration;

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
    return protect(*_worldConfiguration)->allowAccessToClosedShadowRoots();
}

- (void)setOpenClosedShadowRootsEnabled:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowAccessToClosedShadowRoots(allow);
}

- (BOOL)autofillScriptingEnabled
{
    return protect(*_worldConfiguration)->allowAutofill();
}

- (void)setAutofillScriptingEnabled:(BOOL)enabled
{
    protect(*_worldConfiguration)->setAllowAutofill(enabled);
}

- (BOOL)elementUserInfoEnabled
{
    return protect(*_worldConfiguration)->allowElementUserInfo();
}

- (void)setElementUserInfoEnabled:(BOOL)enabled
{
    protect(*_worldConfiguration)->setAllowElementUserInfo(enabled);
}

- (BOOL)legacyBuiltinOverridesEnabled
{
    return !protect(*_worldConfiguration)->disableLegacyBuiltinOverrides();
}

- (void)setLegacyBuiltinOverridesEnabled:(BOOL)enabled
{
    protect(*_worldConfiguration)->setDisableLegacyBuiltinOverrides(!enabled);
}

- (BOOL)jsHandleCreationEnabled
{
    return protect(*_worldConfiguration)->allowJSHandleCreation();
}

- (void)setJSHandleCreationEnabled:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowJSHandleCreation(allow);
}

- (BOOL)isInspectable
{
    return protect(*_worldConfiguration)->isInspectable();
}

- (void)setInspectable:(BOOL)inspectable
{
    protect(*_worldConfiguration)->setInspectable(inspectable);
}

- (BOOL)nodeSerializationEnabled
{
    return protect(*_worldConfiguration)->allowNodeSerialization();
}

- (void)setNodeSerializationEnabled:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowNodeSerialization(allow);
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
    return protect(*_worldConfiguration)->name().createNSString().autorelease();
}

- (void)setName:(NSString *)name
{
    protect(*_worldConfiguration)->setName(name);
}

- (BOOL)allowAccessToClosedShadowRoots
{
    return protect(*_worldConfiguration)->allowAccessToClosedShadowRoots();
}

- (void)setAllowAccessToClosedShadowRoots:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowAccessToClosedShadowRoots(allow);
}

- (BOOL)allowAutofill
{
    return protect(*_worldConfiguration)->allowAutofill();
}

- (void)setAllowAutofill:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowAutofill(allow);
}

- (BOOL)allowElementUserInfo
{
    return protect(*_worldConfiguration)->allowElementUserInfo();
}

- (void)setAllowElementUserInfo:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowElementUserInfo(allow);
}

- (BOOL)disableLegacyBuiltinOverrides
{
    return protect(*_worldConfiguration)->disableLegacyBuiltinOverrides();
}

- (void)setDisableLegacyBuiltinOverrides:(BOOL)disable
{
    protect(*_worldConfiguration)->setDisableLegacyBuiltinOverrides(disable);
}

- (BOOL)allowJSHandleCreation
{
    return protect(*_worldConfiguration)->allowJSHandleCreation();
}

- (void)setAllowJSHandleCreation:(BOOL)allow
{
    protect(*_worldConfiguration)->setAllowJSHandleCreation(allow);
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
