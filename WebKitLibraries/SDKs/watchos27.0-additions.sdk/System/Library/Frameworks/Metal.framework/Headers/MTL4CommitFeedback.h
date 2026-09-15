//
//  MTL4CommitFeedback.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4CommitFeedback_h
#define MTL4CommitFeedback_h

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>


NS_ASSUME_NONNULL_BEGIN

@protocol MTL4CommitFeedback;

/// Defines the block signature for a callback Metal invokes to provide your app feedback after completing a workload.
///
/// You register a commit feedback block with Metal by providing an instance of ``MTL4CommitOptions`` to
/// the command queue's commit method, ``MTL4CommandQueue/commit:count:options:``. The commit options instance
/// references your commit feedback handler after you add it via its ``MTL4CommitOptions/addFeedbackHandler:``
/// method.
///
/// - Parameter commitFeedback: a commit feedback instance containing information about the workload.
typedef void (^ NS_SWIFT_SENDABLE MTL4CommitFeedbackHandler)(id<MTL4CommitFeedback> commitFeedback) API_AVAILABLE(macos(26.0), ios(26.0));

/// Describes an object containing debug information from Metal to your app after completing a workload.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4CommitFeedback <NSObject>

/// A description of an error when the GPU encounters an issue as it runs the committed command buffers.
@property (readonly, nonatomic, nullable) NSError *error;

/// The host time, in seconds, when the GPU starts execution of the committed command buffers.
@property (readonly, nonatomic) CFTimeInterval GPUStartTime;

/// The host time, in seconds, when the GPU finishes execution of the committed command buffers.
@property (readonly, nonatomic) CFTimeInterval GPUEndTime;

@end

NS_ASSUME_NONNULL_END


#endif // MTL4CommitFeedback_h
