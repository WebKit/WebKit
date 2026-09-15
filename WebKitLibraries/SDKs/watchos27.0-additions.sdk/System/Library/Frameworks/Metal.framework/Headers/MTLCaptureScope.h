//
//  MTLCaptureScope.h
//  Metal
//
//  Copyright © 2017 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>


NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@protocol MTLCommandQueue;

@protocol MTL4CommandQueue;

API_AVAILABLE(macos(10.13), ios(11.0))
@protocol MTLCaptureScope <NSObject>

// Remarks: only MTLCommandBuffers created after -[beginScope] and committed before -[endScope] are captured.

// Marks the begin of the capture scope. Note: This method should be invoked repeatedly per frame.
- (void)beginScope;
// Marks the end of the capture scope. Note: This method should be invoked repeatedly per frame.
- (void)endScope;

/** Scope label
    @remarks Created capture scopes are listed in Xcode when long-pressing the capture button, performing the capture over the selected scope
  */
@property (nullable, copy, atomic) NSString *label;

// Associated device: this scope will capture Metal commands from the associated device
@property (nonnull, readonly, nonatomic)  id<MTLDevice> device;
/** If set, this scope will only capture Metal commands from the associated command queue. Defaults to nil (all command queues from the associated device are captured).
 */
@property (nullable, readonly, nonatomic) id<MTLCommandQueue> commandQueue;

/** If set, this scope will only capture Metal commands from the associated Metal 4 command queue. Defaults to nil (all command queues from the associated device are captured).
 */
@property (nullable, readonly, nonatomic) id<MTL4CommandQueue> mtl4CommandQueue;

@end

NS_ASSUME_NONNULL_END

