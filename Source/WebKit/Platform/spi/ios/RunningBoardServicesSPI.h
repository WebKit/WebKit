/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

#if USE(APPLE_INTERNAL_SDK)

#import <RunningBoardServices/RunningBoardServices.h>

extern const NSTimeInterval RBSProcessTimeLimitationNone;

#if __has_include(<RunningBoardServices/RBSProcessLimitations.h>)
#import <RunningBoardServices/RBSProcessLimitations.h>
#else
@interface RBSProcessLimitations : NSObject
@property (nonatomic, readwrite, assign) NSTimeInterval runTime;
@end
#endif

#else

NS_ASSUME_NONNULL_BEGIN

@interface RBSAttribute : NSObject
@end

@interface RBSDomainAttribute : RBSAttribute
+ (instancetype)attributeWithDomain:(NSString *)domain name:(NSString *)name;
@end

@interface RBSTarget : NSObject
+ (RBSTarget *)targetWithPid:(pid_t)pid;
+ (RBSTarget *)currentProcess;
@end

@protocol RBSAssertionObserving;

@interface RBSAssertion : NSObject
- (instancetype)initWithExplanation:(NSString *)explanation target:(RBSTarget *)target attributes:(NSArray <RBSAttribute *> *)attributes;
- (BOOL)acquireWithError:(NSError **)error;
- (void)invalidate;
- (void)addObserver:(id <RBSAssertionObserving>)observer;
- (void)removeObserver:(id <RBSAssertionObserving>)observer;

@property (nonatomic, readonly, assign, getter=isValid) BOOL valid;
@end

@protocol RBSAssertionObserving <NSObject>
- (void)assertionWillInvalidate:(RBSAssertion *)assertion;
- (void)assertion:(RBSAssertion *)assertion didInvalidateWithError:(NSError *)error;
@end

@interface RBSProcessIdentifier : NSObject
+ (RBSProcessIdentifier *)identifierWithPid:(pid_t)pid;
@end

typedef NS_ENUM(uint8_t, RBSTaskState) {
    RBSTaskStateUnknown                 = 0,
    RBSTaskStateNone                    = 1,
    RBSTaskStateRunningUnknown          = 2,
    RBSTaskStateRunningSuspended        = 3,
    RBSTaskStateRunningScheduled        = 4,
};

@interface RBSProcessState : NSObject
@property (nonatomic, readonly, assign) RBSTaskState taskState;
@property (nonatomic, readonly, copy) NSSet<NSString *> *endowmentNamespaces;
@end

extern const NSTimeInterval RBSProcessTimeLimitationNone;

@interface RBSProcessLimitations : NSObject
@property (nonatomic, readwrite, assign) NSTimeInterval runTime;
@end

@interface RBSProcessHandle : NSObject
+ (RBSProcessHandle *)handleForIdentifier:(RBSProcessIdentifier *)identifier error:(NSError **)outError;
+ (RBSProcessHandle *)currentProcess;
@property (nonatomic, readonly, assign) pid_t pid;
@property (nonatomic, readonly, strong) RBSProcessState *currentState;
@property (nonatomic, readonly, strong) RBSProcessLimitations *activeLimitations;
@end

@interface RBSProcessStateUpdate : NSObject
@property (nonatomic, readonly, strong) RBSProcessHandle *process;
@property (nonatomic, readonly, strong, nullable) RBSProcessState *state;
@end

@class RBSProcessMonitor;
@class RBSProcessPredicate;
@class RBSProcessStateDescriptor;
@protocol RBSProcessMonitorConfiguring;

typedef void (^RBSProcessMonitorConfigurator)(id<RBSProcessMonitorConfiguring> config);
typedef void (^RBSProcessUpdateHandler)(RBSProcessMonitor *monitor, RBSProcessHandle *process, RBSProcessStateUpdate *update);

@protocol RBSProcessMatching <NSObject>
- (RBSProcessPredicate *)processPredicate;
@end

@protocol RBSProcessMonitorConfiguring
- (void)setPredicates:(nullable NSArray<RBSProcessPredicate *> *)predicates;
- (void)setStateDescriptor:(nullable RBSProcessStateDescriptor *)descriptor;
- (void)setUpdateHandler:(nullable RBSProcessUpdateHandler)block;
@end

@interface RBSProcessMonitor : NSObject <NSCopying>
+ (instancetype)monitorWithConfiguration:(NS_NOESCAPE RBSProcessMonitorConfigurator)block;
@end

@interface RBSProcessPredicate : NSObject <RBSProcessMatching>
+ (RBSProcessPredicate *)predicateMatchingHandle:(RBSProcessHandle *)process;
@end

@interface RBSProcessStateDescriptor : NSObject <NSCopying>
+ (instancetype)descriptor;
@property (nonatomic, readwrite, copy, nullable) NSArray<NSString *> *endowmentNamespaces;
@end

NS_ASSUME_NONNULL_END

#endif
