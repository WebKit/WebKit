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

#if PLATFORM(IOS) || PLATFORM(VISION)
#if USE(APPLE_INTERNAL_SDK)

#import <InstallCoordination/IXAppInstallCoordinator.h>
#import <InstallCoordination/InstallCoordination.h>

#else

#include "LaunchServicesSPI.h"

NS_ASSUME_NONNULL_BEGIN

typedef enum : NSUInteger {
} IXCoordinatorProgressPhase;

typedef enum : NSUInteger {
    IXAppInstallCoordinatorPlaceholderPromise = 1,
} IXAppInstallCoordinatorPromiseIdentifier;

typedef enum : NSUInteger {
    IXClientIdentifierAppliedFor = 19,
} IXClientIdentifier;

@class LSApplicationRecord;
@class IXAppInstallCoordinator;
@class IXPlaceholder;
@class IXPromisedOutOfBandTransfer;

@protocol IXAppInstallCoordinatorObserver <NSObject>
@optional
- (void)coordinatorDidRegisterForObservation:(nonnull IXAppInstallCoordinator *)coordinator;
- (void)coordinatorShouldPrioritize:(nonnull IXAppInstallCoordinator *)coordinator;
- (void)coordinatorShouldResume:(nonnull IXAppInstallCoordinator *)coordinator;
- (void)coordinatorShouldPause:(nonnull IXAppInstallCoordinator *)coordinator;
- (void)coordinator:(nonnull IXAppInstallCoordinator *)coordinator didUpdateProgress:(double)percentComplete forPhase:(IXCoordinatorProgressPhase)phase overallProgress:(double)overallProgress;
- (void)coordinatorDidCompleteSuccessfully:(nonnull IXAppInstallCoordinator *)coordinator forApplicationRecord:(nullable LSApplicationRecord *)appRecord;
- (void)coordinatorDidInstallPlaceholder:(nonnull IXAppInstallCoordinator *)coordinator forApplicationRecord:(nullable LSApplicationRecord *)placeholderRecord;
- (void)coordinator:(nonnull IXAppInstallCoordinator *)coordinator canceledWithReason:(nonnull NSError *)cancelReason client:(IXClientIdentifier)client;
@end

typedef void (^IXAppInstallCoordinatorErrorCompletion)(NSError * _Nullable error);

@interface IXAppInstallCoordinator
+ (void)uninstallAppWithBundleID:(NSString *)bundleID requestUserConfirmation:(BOOL)requestUserConfirmation completion:(IXAppInstallCoordinatorErrorCompletion)completion;
+ (BOOL)cancelCoordinatorForAppWithBundleID:(NSString *)bundleID withReason:(NSError *)cancelReason client:(IXClientIdentifier)client error:(NSError **)out_error;
+ (instancetype)coordinatorForAppWithBundleID:(NSString *)bundleID withClientID:(IXClientIdentifier)identity createIfNotExisting:(BOOL)create created:(BOOL *)out_created error:(NSError **)out_error;
@property (nonatomic, nullable, weak) id<IXAppInstallCoordinatorObserver> observer;
- (BOOL)setPlaceholderPromise:(IXPlaceholder *)placeholderPromise error:(NSError **)out_error;
- (BOOL)setUserDataPromise:(IXPromisedOutOfBandTransfer *)userData error:(NSError **)out_error;
@end

@interface IXRestoringDemotedAppInstallCoordinator : IXAppInstallCoordinator
@end

@interface IXOwnedDataPromise : NSObject
@end

@interface IXPromisedInMemoryDictionary : IXOwnedDataPromise
- (instancetype)initWithName:(NSString *)name client:(IXClientIdentifier)identity dictionary:(NSDictionary *)dictionary;
@end

@interface IXPlaceholderAttributes : NSObject
@property (nonatomic) BOOL launchProhibited;
@end

@interface IXPlaceholder : NSObject
- (instancetype)initAppPlaceholderWithBundleName:(NSString *)name bundleID:(NSString *)bundleID installType:(LSInstallType)installType client:(IXClientIdentifier)identity NS_DESIGNATED_INITIALIZER;
- (BOOL)setPlaceholderAttributes:(nonnull IXPlaceholderAttributes *)attributes error:(NSError **)out_error;
- (BOOL)setEntitlementsPromise:(nonnull IXOwnedDataPromise *)entitlements error:(NSError * __autoreleasing _Nullable * _Nullable)out_error;
- (BOOL)setConfigurationCompleteWithError:(NSError **)out_error;
@end

@interface IXOpaqueDataPromise : NSObject
@end

@interface IXPromisedOutOfBandTransfer : IXOpaqueDataPromise
@property (nonatomic) BOOL complete;
@property (nonatomic) double percentComplete;
- (instancetype)initWithName:(NSString *)name client:(IXClientIdentifier)identity diskSpaceNeeded:(uint64_t)totalBytes;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)
#endif // PLATFORM(IOS) || PLATFORM(VISION)
