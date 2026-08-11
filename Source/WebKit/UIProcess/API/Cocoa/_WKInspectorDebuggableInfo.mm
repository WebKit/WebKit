/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "_WKInspectorDebuggableInfoInternal.h"

#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKInspectorDebuggableInfo

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::DebuggableInfo>(self);

    return self;
}

- (_WKInspectorDebuggableType)debuggableType
{
    return toWKInspectorDebuggableType(_debuggableInfo->debuggableType());
}

- (void)setDebuggableType:(_WKInspectorDebuggableType)debuggableType
{
    _debuggableInfo->setDebuggableType(fromWKInspectorDebuggableType(debuggableType));
}

- (NSString *)targetPlatformName
{
    return _debuggableInfo->targetPlatformName().createNSString().autorelease();
}

- (void)setTargetPlatformName:(NSString *)targetPlatformName
{
    protect(*_debuggableInfo)->setTargetPlatformName(targetPlatformName);
}

- (NSString *)targetBuildVersion
{
    return _debuggableInfo->targetBuildVersion().createNSString().autorelease();
}

- (void)setTargetBuildVersion:(NSString *)targetBuildVersion
{
    protect(*_debuggableInfo)->setTargetBuildVersion(targetBuildVersion);
}

- (NSString *)targetProductVersion
{
    return _debuggableInfo->targetProductVersion().createNSString().autorelease();
}

- (void)setTargetProductVersion:(NSString *)targetProductVersion
{
    protect(*_debuggableInfo)->setTargetProductVersion(targetProductVersion);
}

- (BOOL)targetIsSimulator
{
    return _debuggableInfo->targetIsSimulator();
}

- (void)setTargetIsSimulator:(BOOL)targetIsSimulator
{
    _debuggableInfo->setTargetIsSimulator(targetIsSimulator);
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKInspectorDebuggableInfo.class, self))
        return;

    SUPPRESS_UNRETAINED_ARG _debuggableInfo->~DebuggableInfo();

    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    RetainPtr<_WKInspectorDebuggableInfo> debuggableInfo = adoptNS([(_WKInspectorDebuggableInfo *)[[self class] allocWithZone:zone] init]);

    debuggableInfo.get().debuggableType = self.debuggableType;
    debuggableInfo.get().targetPlatformName = self.targetPlatformName;
    debuggableInfo.get().targetBuildVersion = self.targetBuildVersion;
    debuggableInfo.get().targetProductVersion = self.targetProductVersion;
    debuggableInfo.get().targetIsSimulator = self.targetIsSimulator;

    return debuggableInfo.leakRef();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_debuggableInfo;
}

@end
