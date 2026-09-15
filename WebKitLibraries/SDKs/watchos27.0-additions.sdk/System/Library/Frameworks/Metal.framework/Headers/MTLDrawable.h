//
//  MTLDrawable.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDrawable;

/*!
 @typedef MTLDrawablePresentedHandler
 @abstract The presented callback function protocol
 @discussion Be careful when you use delta between this presentedTime and previous frame's presentedTime to animate next frame. If the frame was presented using presentAfterMinimumDuration or presentAtTime, the presentedTime might includes delays to meet your specified present time. If you want to measure how much frame you can achieve, use GPUStartTime in the first command buffer of your frame rendering and GPUEndTime of your last frame rendering to calculate the frame interval.
*/
typedef void (^ NS_SWIFT_SENDABLE MTLDrawablePresentedHandler)(id<MTLDrawable>);

/*!
 @protocol MTLDrawable
 @abstract All "drawable" objects (such as those coming from CAMetalLayer) are expected to conform to this protocol
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLDrawable <NSObject>

/* Present this drawable as soon as possible */
- (void)present;

/* Present this drawable at the given host time */
- (void)presentAtTime:(CFTimeInterval)presentationTime;

/*!
 @method presentAfterMinimumDuration
 @abstract Present this drawable while setting a minimum duration in seconds before allowing this drawable to appear on the display.
 @param duration Duration in seconds before this drawable is allowed to appear on the display
 */
- (void)presentAfterMinimumDuration:(CFTimeInterval)duration API_AVAILABLE(macos(10.15.4), ios(10.3), macCatalyst(13.4));

/*!
 @method addPresentedHandler
 @abstract Add a block to be called when this drawable is presented on screen.
 */
- (void)addPresentedHandler:(MTLDrawablePresentedHandler)block API_AVAILABLE(macos(10.15.4), ios(10.3), macCatalyst(13.4));

/*!
 @property presentedTime
 @abstract The host time that this drawable was presented on screen.
 @discussion Returns 0 if a frame has not been presented or has been skipped.
 */
@property(nonatomic, readonly) CFTimeInterval presentedTime API_AVAILABLE(macos(10.15.4), ios(10.3), macCatalyst(13.4));

/*!
 @property drawableID
 @abstract The monotonically incremented ID for all MTLDrawable objects created from the same CAMetalLayer object.
  @discussion The value starts from 0.
*/
@property (nonatomic, readonly) NSUInteger drawableID API_AVAILABLE(macos(10.15.4), ios(10.3), macCatalyst(13.4));

@end
NS_ASSUME_NONNULL_END
