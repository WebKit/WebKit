//
//  MTL4CompilerTask.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4CompilerTask_h
#define MTL4CompilerTask_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTL4Compiler;

/// Represents the status of a compiler task.
API_AVAILABLE(macos(26.0), ios(26.0))
typedef NS_ENUM(NSInteger, MTL4CompilerTaskStatus) {
    /// No status.
    MTL4CompilerTaskStatusNone      = 0,
    
    /// The compiler task is currently scheduled.
    MTL4CompilerTaskStatusScheduled = 1,
    
    /// The compiler task is currently compiling.
    MTL4CompilerTaskStatusCompiling = 2,
    
    /// The compiler task is finished.
    MTL4CompilerTaskStatusFinished  = 3,
};

/// A reference to an asynchronous compilation task that you initiate from a compiler instance.
API_AVAILABLE(macos(26.0), ios(26.0))
NS_SWIFT_UNAVAILABLE("Use Swift Task instead.")
@protocol MTL4CompilerTask <NSObject>
/// Returns the compiler instance that this asynchronous compiler task belongs to.
@property (readonly) id<MTL4Compiler> compiler;

/// Returns the compiler task status.
///
/// The default is `MTL4CompilerStatusNone`.
@property (readonly) MTL4CompilerTaskStatus status;

/// Waits synchronously for this compile task to complete by blocking the calling thread.
- (void)waitUntilCompleted;


@end

NS_ASSUME_NONNULL_END

#endif // MTL4CompilerTask_h
