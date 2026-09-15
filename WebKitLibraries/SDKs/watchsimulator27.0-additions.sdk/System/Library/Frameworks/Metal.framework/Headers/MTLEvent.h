//
//  MTLEvent.h
//  Metal
//
//  Copyright © 2018 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLDevice.h>

NS_ASSUME_NONNULL_BEGIN

API_AVAILABLE(macos(10.14), ios(12.0)) NS_SWIFT_SENDABLE
@protocol MTLEvent <NSObject>

/*!
 @property device
 @abstract The device this event can be used with. Will be nil when the event is shared across devices (i.e. MTLSharedEvent).
 */
@property (nullable, readonly) id <MTLDevice> device;

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label;

@end

/*!
 @class MTLSharedEventListener
 @abstract This class provides a simple interface for handling the dispatching of MTLSharedEvent notifications from Metal.
*/
MTL_EXPORT API_AVAILABLE(macos(10.14), ios(12.0)) NS_SWIFT_SENDABLE
@interface MTLSharedEventListener : NSObject
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithDispatchQueue:(dispatch_queue_t)dispatchQueue NS_DESIGNATED_INITIALIZER;
@property (nonnull, readonly) dispatch_queue_t dispatchQueue;

// A shared instance constructed with a standard serial dispatch queue.
// This instance can be used for short-running notifications without QoS requirements.
+ (MTLSharedEventListener*)sharedListener API_AVAILABLE(macos(26.0), ios(26.0));
@end

@class MTLSharedEventHandle;
@protocol MTLSharedEvent;

typedef void (^ NS_SWIFT_SENDABLE MTLSharedEventNotificationBlock)(id <MTLSharedEvent>, uint64_t value);

API_AVAILABLE(macos(10.14), ios(12.0))
@protocol MTLSharedEvent <MTLEvent>

// When the event's signaled value reaches value or higher, invoke the block on the dispatch queue owned by the listener.
- (void)notifyListener:(MTLSharedEventListener *)listener atValue:(uint64_t)value block:(MTLSharedEventNotificationBlock)block;

// Convenience method for creating a shared event handle that may be passed to other processes via XPC.
- (MTLSharedEventHandle *)newSharedEventHandle;

// Synchronously wait for the signaledValue to be greater than or equal to 'value', with a timeout
// specified in milliseconds.   Returns YES if the value was signaled before the timeout, otherwise NO.
- (BOOL)waitUntilSignaledValue:(uint64_t)value timeoutMS:(uint64_t)milliseconds API_AVAILABLE(macos(12.0), ios(15.0)) NS_SWIFT_UNAVAILABLE_FROM_ASYNC("Use 'await valueSignaled(...)' instead.");

@property (readwrite) uint64_t signaledValue; // Read or set signaled value

@end


// MTLSharedEventHandle objects may be passed between processes via XPC connections and then used to recreate
// a MTLSharedEvent via an existing MTLDevice.
MTL_EXPORT API_AVAILABLE(macos(10.14), ios(12.0)) NS_SWIFT_SENDABLE
@interface MTLSharedEventHandle : NSObject <NSSecureCoding>
{
    struct MTLSharedEventHandlePrivate *_priv;
}
@property (readonly, nullable) NSString *label;
@end

NS_ASSUME_NONNULL_END
