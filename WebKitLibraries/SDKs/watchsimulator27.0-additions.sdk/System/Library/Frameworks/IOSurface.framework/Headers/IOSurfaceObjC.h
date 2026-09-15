/*
 *  IOSurfaceObjC.h
 *  IOSurface Objective C Interface
 *
 *  Copyright 2017 Apple Inc. All rights reserved.
 *
 */

#ifndef IOSURFACE_OBJC_H
#define IOSURFACE_OBJC_H 1

#if defined(__OBJC__)

#import <IOSurface/IOSurfaceTypes.h>
#import <IOSurface/IOSurfaceRef.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#if !defined(NS_STRING_ENUM)
#define NS_STRING_ENUM
#endif

typedef NSString *IOSurfacePropertyKey NS_STRING_ENUM;

/* The following list of properties are used with the CFDictionary passed to IOSurfaceCreate(). */

/* IOSurfacePropertyKeyAllocSize    - NSNumber of the total allocation size of the buffer including all planes.    
   Defaults to BufferHeight * BytesPerRow if not specified.   Must be specified for
   dimensionless buffers. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyAllocSize                   API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0));

/* IOSurfacePropertyKeyWidth  - NSNumber for the width of the IOSurface buffer in pixels.   Required for planar IOSurfaces. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyWidth                       API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyHeight - NSNumber for the height of the IOSurface buffer in pixels.  Required for planar IOSurfaces. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyHeight                      API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyBytesPerRow - NSNumber for the bytes per row of the buffer.   If not specified, IOSurface will first calculate
   the number full elements required on each row (by rounding up), multiplied by the bytes per element for this surface.
   That value will then be appropriately aligned. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyBytesPerRow                 API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* Optional properties for non-planar two dimensional images */
 
/* IOSurfacePropertyKeyBytesPerElement - NSNumber for the total number of bytes in an element.  Default to 1. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyBytesPerElement             API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyElementWidth   - NSNumber for how many pixels wide each element is.   Defaults to 1. */ 
extern IOSurfacePropertyKey IOSurfacePropertyKeyElementWidth                API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyElementHeight  - NSNumber for how many pixels high each element is.   Defaults to 1. */ 
extern IOSurfacePropertyKey IOSurfacePropertyKeyElementHeight               API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyOffset - NSNumber for the starting offset into the buffer.  Defaults to 0. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyOffset                      API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* Properties for planar surface buffers */

/* IOSurfacePropertyKeyPlaneInfo    - NSArray describing each image plane in the buffer as a CFDictionary.   The CFArray must have at least one entry. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneInfo                   API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneWidth  - NSNumber for the width of this plane in pixels.  Required for image planes. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneWidth                  API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneHeight  - NSNumber for the height of this plane in pixels.  Required for image planes. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneHeight                 API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneBytesPerRow    - NSNumber for the bytes per row of this plane.  If not specified, IOSurface will first calculate
   the number full elements required on each row (by rounding up), multiplied by the bytes per element for this plane.  
   That value will then be appropriately aligned. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneBytesPerRow            API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneOffset  - NSNumber for the offset into the buffer for this plane.  If not specified then IOSurface
   will lay out each plane sequentially based on the previous plane's allocation size. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneOffset                 API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneSize    - NSNumber for the total data size of this plane.  Defaults to plane height * plane bytes per row if not specified. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneSize                   API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* Optional properties for planar surface buffers */

/* IOSurfacePropertyKeyPlaneBase    - NSNumber for the base offset into the buffer for this plane. Optional, defaults to the plane offset */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneBase                   API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneBytesPerElement    - NSNumber for the bytes per element of this plane.  Optional, default is 1. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneBytesPerElement        API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneElementWidth    - NSNumber for the element width of this plane.  Optional, default is 1. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneElementWidth           API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPlaneElementHeight   - NSNumber for the element height of this plane.  Optional, default is 1. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPlaneElementHeight          API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* Optional properties global to the entire IOSurface */

/* IOSurfacePropertyKeyCacheMode        - NSNumber for the CPU cache mode to be used for the allocation.  Default is kIOMapDefaultCache. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyCacheMode                   API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPixelFormat - NSNumber   A 32-bit unsigned integer that stores the traditional Mac OS X buffer format  */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPixelFormat                 API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyPixelSizeCastingAllowed - If false the creator promises that there will be no pixel size casting when used on the GPU.  Default is true.  */
extern IOSurfacePropertyKey IOSurfacePropertyKeyPixelSizeCastingAllowed     API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* IOSurfacePropertyKeyName - Provide a name for the surface at creation time. */
extern IOSurfacePropertyKey IOSurfacePropertyKeyName                        API_AVAILABLE(macos(13.0), ios(16.0), watchos(9.0), tvos(16.0));

// Note: IOSurface objects are "toll free bridged" to IOSurfaceRef objects
API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0))
NS_SWIFT_SENDABLE
@interface IOSurface : NSObject <NSSecureCoding>
{
@package
    void * _impl;
}

/* Create a new IOSurface */
- (nullable instancetype)initWithProperties:(NSDictionary <IOSurfacePropertyKey, id NS_SWIFT_SENDABLE> *)properties;

/* "Lock" or "Unlock" a IOSurface for reading or writing.

    The term "lock" is used loosely in this context, and is simply used along with the
    "unlock" information to put a bound on CPU access to the raw IOSurface data.
    
    If the seed parameter is non-NULL, IOSurfaceLock() will store the buffer's
    internal modification seed value at the time you made the lock call.   You can compare
    this value to a value returned previously to determine of the contents of the buffer
    has been changed since the last lock.
    
    In the case of IOSurfaceUnlock(), the seed value returned will be the internal
    seed value at the time of the unlock.  If you locked the buffer for writing, this value
    will be incremented as the unlock is performed and the new value will be returned.
    
    See the IOSurfacePropertyKeyLock enums for more information.
    
    Note: Locking and unlocking a IOSurface is not a particularly cheap operation,
    so care should be taken to avoid the calls whenever possible.   The seed values are 
    particularly useful for keeping a cache of the buffer contents.
*/
- (kern_return_t)lockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed;
- (kern_return_t)unlockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed;

/* The total allocation size of the IOSurface */
@property (readonly) NSInteger allocationSize;
/* Basic surface layout information */
@property (readonly) NSInteger width;
@property (readonly) NSInteger height;
@property (readonly) void *baseAddress NS_RETURNS_INNER_POINTER;
@property (readonly) OSType pixelFormat;
/* Note: These properties may not return well-defined values for planar surfaces */
@property (readonly) NSInteger bytesPerRow;
@property (readonly) NSInteger bytesPerElement;
@property (readonly) NSInteger elementWidth;
@property (readonly) NSInteger elementHeight;
@property (readonly) uint32_t surfaceID API_AVAILABLE(macos(15.0), ios(18.0), watchos(11.0), tvos(18.0));

/* This will return the current seed value of the buffer and is a cheap property to read to see
   if the contents of the buffer have changed since the last lock/unlock. */
@property (readonly) uint32_t seed;

/* Return the number of planes in this buffer.  Will be 0 if the surface is non-planar */
@property (readonly) NSUInteger planeCount;

/* These properties return information about a particular plane of a IOSurface.  They will
   raise if called on non-planar surfaces or if the index value is not less than the number
   of planes. */
- (NSInteger)widthOfPlaneAtIndex:(NSUInteger)planeIndex;
- (NSInteger)heightOfPlaneAtIndex:(NSUInteger)planeIndex;
- (NSInteger)bytesPerRowOfPlaneAtIndex:(NSUInteger)planeIndex;
- (NSInteger)bytesPerElementOfPlaneAtIndex:(NSUInteger)planeIndex;
- (NSInteger)elementWidthOfPlaneAtIndex:(NSUInteger)planeIndex;
- (NSInteger)elementHeightOfPlaneAtIndex:(NSUInteger)planeIndex;
- (void *)baseAddressOfPlaneAtIndex:(NSUInteger)planeIndex NS_RETURNS_INNER_POINTER;

/* These calls let you attach property list types to a IOSurface buffer.  These calls are 
   expensive (they essentially must serialize the data into the kernel) and thus should be avoided whenever
   possible.   Note:  These functions can not be used to change the underlying surface properties. */
- (void)setAttachment:(id NS_SWIFT_SENDABLE)anObject forKey:(NSString *)key;
- (nullable id NS_SWIFT_SENDABLE)attachmentForKey:(NSString *)key;
- (void)removeAttachmentForKey:(NSString *)key;
- (void)setAllAttachments:(NSDictionary<NSString *, id NS_SWIFT_SENDABLE> *)dict;
- (nullable NSDictionary<NSString *, id NS_SWIFT_SENDABLE> *)allAttachments;
- (void)removeAllAttachments;

/* There are cases where it is useful to know whether or not an IOSurface buffer is considered to be "in use"
   by other users of the same IOSurface.  In particular, CoreVideo and other APIs make use of the IOSurface
   use count facility to know when it is safe to recycle an IOSurface backed CVPixelBuffer object.  This is
   particularly important when IOSurface objects are being shared across process boundaries and the normal
   mechanisms one might use would not be viable.
   
   The IOSurface use count is similar in concept to any other reference counting scheme.  When the global use
   count of an IOSurface goes to zero, it is no longer considered "in use".   When it is anything other than
   zero, then the IOSurface is still "in use" by someone and therefore anyone attempting to maintain a pool
   of IOSurfaces to be recycled should not reclaim that IOSurface.
   
   Note that IOSurface maintains both a per-process and an internal system wide usage count.   In the current
   implementation, when the per-process usage count goes from zero to one, the system wide usage count is
   incremented by one.   When the per-process usage count drops back to zero (either via explicit decrement
   calls or the process terminates), the global usage count is decremented by one. */
   
@property (readonly, getter = isInUse) BOOL inUse;
- (void)incrementUseCount;
- (void)decrementUseCount;

/* The localUseCount property returns the local per-process usage count for an IOSurface.  This call is only
   provided for logging/debugging purposes and should never be used to determine whether an IOSurface is
   considered to be "in use".   The isInUse property is the only call that should be used for that purpose. */
@property (readonly ) int32_t localUseCount;

/* This property returns YES if it is legal to choose an OpenGL or Metal pixel format with a bytes per pixel
value that is different than the bytesPerElement value(s) of this IOSurface.  Returns NO if the bytes per pixel
value must be an exact match. */
@property (readonly) BOOL allowsPixelSizeCasting;

/* See comments in IOSurfaceAPI.h */
- (kern_return_t)setPurgeable:(IOSurfacePurgeabilityState)newState oldState:(IOSurfacePurgeabilityState * __nullable)oldState
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

@end 

// This key was misnamed.
extern IOSurfacePropertyKey IOSurfacePropertyAllocSizeKey
    API_DEPRECATED_WITH_REPLACEMENT("IOSurfacePropertyKeyAllocSize",macos(10.12,10.14),ios(11.0,12.0),watchos(4.0,5.0),tvos(11.0,12.0));

static inline __attribute__((always_inline)) IOSurface *_IOSurfaceRefToObj(IOSurfaceRef ref) NS_SWIFT_NAME(IOSurface.init(_:))
{
    return (__bridge IOSurface *)ref;
}

static inline __attribute__((always_inline)) IOSurfaceRef _IOSurfaceObjToRef(IOSurface *obj) NS_SWIFT_NAME(IOSurfaceRef.init(_:))
{
    return (__bridge IOSurfaceRef)obj;
}

NS_ASSUME_NONNULL_END

#endif

#endif
