/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#if USE(EXTENSIONKIT)

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#if __has_include(<ServiceExtensions/ServiceExtensions_Private.h>)
#import <ServiceExtensions/ServiceExtensions_Private.h>
#else

@class _SEContentProcess;
@class _SEGPUProcess;
@class _SENetworkProcess;
@protocol UIInteraction;

NS_ASSUME_NONNULL_BEGIN

@interface _SEServiceConfiguration : NSObject
typedef void(^_SEServiceInteruptionHandler)();
@property (copy) NSString *serviceIdentifier;
@property (copy) _SEServiceInteruptionHandler interuptionHandler;

- (instancetype)initWithServiceIdentifier:(NSString *)serviceIdentifier;

@end

@interface _SEServiceManager: NSObject

+ (instancetype)sharedInstance;

- (void)contentProcessWithConfiguration:(_SEServiceConfiguration *)configuration completion:(void(^)(_SEContentProcess * _Nullable process, NSError * _Nullable error))completion;

- (void)networkProcessWithConfiguration:(_SEServiceConfiguration *)configuration completion:(void(^)(_SENetworkProcess * _Nullable process, NSError * _Nullable error))completion;

- (void)gpuProcessWithConfiguration:(_SEServiceConfiguration *)configuration completion:(void(^)(_SEGPUProcess * _Nullable process, NSError * _Nullable error))completion;

@end

#endif

#if __has_include(<ServiceExtensions/SEProcesses_Private.h>)
#import <ServiceExtensions/SEProcesses_Private.h>
#else

@protocol _SEGrant <NSObject>
- (BOOL)invalidateWithError:(NSError * _Nullable *)error;
@property (readonly) BOOL isValid;
@end

@interface _SECapability : NSObject
@end

NS_REFINED_FOR_SWIFT
@interface _SEExtensionProcess : NSObject

- (nullable xpc_connection_t)makeLibXPCConnectionError:(NSError * _Nullable *)error;

- (void)invalidate;

- (nullable id<_SEGrant>)grantCapability:(_SECapability *)capability error:(NSError * _Nullable *)error;

@end

NS_REFINED_FOR_SWIFT
@interface _SEContentProcess : _SEExtensionProcess
- (id<UIInteraction>)createVisibilityPropagationInteraction;
@end

NS_REFINED_FOR_SWIFT
@interface _SEGPUProcess : _SEExtensionProcess
- (id<UIInteraction>)createVisibilityPropagationInteraction;
@end

NS_REFINED_FOR_SWIFT
@interface _SENetworkProcess : _SEExtensionProcess
@end

NS_ASSUME_NONNULL_END

#endif

NS_ASSUME_NONNULL_BEGIN

@interface _SECapability (SPI)
- (BOOL)setActive:(BOOL)active;
+ (instancetype)mediaWithWebsite:(NSString *)website;
+ (instancetype)assertionWithDomain:(NSString *)domain name:(NSString *)name;
+ (instancetype)assertionWithDomain:(NSString *)domain name:(NSString *)name environmentIdentifier:(NSString *)environmentIdentifier;
+ (instancetype)assertionWithDomain:(NSString *)domain name:(NSString *)name environmentIdentifier:(NSString *)environmentIdentifier willInvalidate:(void (^)())willInvalidateBlock didInvalidate:(void (^)())didInvalidateBlock;
@property (nonatomic, readonly) NSString *mediaEnvironment;
@end

@interface _SEHostingHandle: NSObject
-(instancetype)initFromXPCRepresentation:(xpc_object_t)xpcRepresentation;
-(xpc_object_t)xpcRepresentation;
@end

@interface _SEHostable: NSObject
+(_SEHostable*)createHostableWithOptions:(NSDictionary*)dict error:(NSError**)error;
@property (nonatomic, readonly) _SEHostingHandle* handle;
@property (nonatomic, strong) CALayer *layer;
@end

@interface _SEHostingView: UIView
@property (nonatomic, retain) _SEHostingHandle* handle;
@end

@interface _SEHostingUpdateCoordinator : NSObject
-(instancetype)init;
-(instancetype)initFromXPCRepresentation:(xpc_object_t)xpcRepresentation;
-(xpc_object_t)xpcRepresentation;
-(void)addHostable:(_SEHostable*)hostable;
-(void)addHostingView:(_SEHostingView*)hostingView;
-(void)commit;
@end

NS_ASSUME_NONNULL_END

#endif // USE(EXTENSIONKIT)
