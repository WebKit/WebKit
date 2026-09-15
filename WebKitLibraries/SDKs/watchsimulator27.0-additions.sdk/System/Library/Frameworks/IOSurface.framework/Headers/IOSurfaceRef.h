/*
 *  IOSurfaceRef.h
 *  IOSurface
 *
 *  Copyright 2006-2017 Apple Inc. All rights reserved.
 *
 */

#ifndef IOSURFACE_REF_H
#define IOSURFACE_REF_H 1

#include <IOSurface/IOSurfaceBase.h>
#include <IOSurface/IOSurfaceTypes.h>

typedef struct CF_BRIDGED_TYPE(id) CF_BRIDGED_MUTABLE_TYPE(IOSurface) __IOSurface *IOSurfaceRef IOSFC_SWIFT_NAME(IOSurfaceRef);

__BEGIN_DECLS

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

/* The following list of properties are used with the CFDictionary passed to IOSurfaceCreate(). */

/* kIOSurfaceAllocSize    - CFNumber of the total allocation size of the buffer including all planes.    
   Defaults to BufferHeight * BytesPerRow if not specified.   Must be specified for
   dimensionless buffers. */
extern const CFStringRef kIOSurfaceAllocSize                                API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceWidth  - CFNumber for the width of the IOSurface buffer in pixels.   Required for planar IOSurfaces. */
extern const CFStringRef kIOSurfaceWidth                                    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceHeight - CFNumber for the height of the IOSurface buffer in pixels.  Required for planar IOSurfaces. */
extern const CFStringRef kIOSurfaceHeight                                   API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceBytesPerRow - CFNumber for the bytes per row of the buffer.   If not specified, IOSurface will first calculate
   the number full elements required on each row (by rounding up), multiplied by the bytes per element
   for this buffer.   That value will then be appropriately aligned. */
extern const CFStringRef kIOSurfaceBytesPerRow                              API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Optional properties for non-planar two dimensional images */
 
/* kIOSurfaceBytesPerElement - CFNumber for the total number of bytes in an element.  Default to 1. */
extern const CFStringRef kIOSurfaceBytesPerElement                          API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceElementWidth   - CFNumber for how many pixels wide each element is.   Defaults to 1. */ 
extern const CFStringRef kIOSurfaceElementWidth                             API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceElementHeight  - CFNumber for how many pixels high each element is.   Defaults to 1. */ 
extern const CFStringRef kIOSurfaceElementHeight                            API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceOffset - CFNumber for the starting offset into the buffer.  Defaults to 0. */
extern const CFStringRef kIOSurfaceOffset                                   API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Properties for planar surface buffers */

/* kIOSurfacePlaneInfo    - CFArray describing each image plane in the buffer as a CFDictionary.   The CFArray must have at least one entry. */
extern const CFStringRef kIOSurfacePlaneInfo                                API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneWidth  - CFNumber for the width of this plane in pixels.  Required for image planes. */
extern const CFStringRef kIOSurfacePlaneWidth                               API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneHeight  - CFNumber for the height of this plane in pixels.  Required for image planes. */
extern const CFStringRef kIOSurfacePlaneHeight                              API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneBytesPerRow    - CFNumber for the bytes per row of this plane.  If not specified, IOSurface will first calculate
   the number full elements required on each row (by rounding up), multiplied by the bytes per element
   for this plane.   That value will then be appropriately aligned. */
extern const CFStringRef kIOSurfacePlaneBytesPerRow                         API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneOffset  - CFNumber for the offset into the buffer for this plane.  If not specified then IOSurface
   will lay out each plane sequentially based on the previous plane's allocation size. */
extern const CFStringRef kIOSurfacePlaneOffset                              API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneSize    - CFNumber for the total data size of this plane.  Defaults to plane height * plane bytes per row if not specified. */
extern const CFStringRef kIOSurfacePlaneSize                                API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Optional properties for planar surface buffers */

/* kIOSurfacePlaneBase    - CFNumber for the base offset into the buffer for this plane. Optional, defaults to the plane offset */
extern const CFStringRef kIOSurfacePlaneBase                                API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneBitsPerElement    - CFNumber for the bits per element of this plane.  Optional, default is 1. 
   For use in cases where kIOSurfacePlaneBytesPerElement doesn't allow sufficient precision. */
extern const CFStringRef kIOSurfacePlaneBitsPerElement                      API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneBytesPerElement    - CFNumber for the bytes per element of this plane.  Optional, default is 1. */
extern const CFStringRef kIOSurfacePlaneBytesPerElement                     API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneElementWidth    - CFNumber for the element width of this plane.  Optional, default is 1. */
extern const CFStringRef kIOSurfacePlaneElementWidth                        API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneElementHeight   - CFNumber for the element height of this plane.  Optional, default is 1. */
extern const CFStringRef kIOSurfacePlaneElementHeight                       API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Optional properties global to the entire IOSurface */

/* kIOSurfaceCacheMode  - CFNumber for the CPU cache mode to be used for the allocation.  Default is kIOMapDefaultCache. */
extern const CFStringRef kIOSurfaceCacheMode                                API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfaceIsGlobal   - CFBoolean     If true, the IOSurface may be looked up by any task in the system by its ID. */
extern const CFStringRef kIOSurfaceIsGlobal                                 API_DEPRECATED("Global surfaces are insecure",macos(10.6,10.11), ios(11.0,11.0), watchos(4.0,4.0), tvos(11.0,11.0));

/* kIOSurfacePixelFormat - CFNumber     A 32-bit unsigned integer that stores the traditional Mac OS X buffer format  */
extern const CFStringRef kIOSurfacePixelFormat                              API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePixelSizeCastingAllowed - If false the creator promises that there will be no pixel size casting when used on the GPU.  Default is true.  */
extern const CFStringRef kIOSurfacePixelSizeCastingAllowed                  API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneComponentBitDepths   - CFArray[CFNumber] for bit depth of each component in this plane. */
extern const CFStringRef kIOSurfacePlaneComponentBitDepths                  API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/* kIOSurfacePlaneComponentBitOffsets   - CFArray[CFNumber] for bit offset of each component in this plane, (low bit zero, high bit 7). For example 'BGRA' would be {0, 8, 16, 24} */
extern const CFStringRef kIOSurfacePlaneComponentBitOffsets                 API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

// This key may be used to specify a name for the IOSurface either at creation time, or may be used with IOSurfaceSetValue() to
// set it dynamically.  If not provided, the name will be set based on the binary containing the address calling into IOSurface.framework
// to create it.
extern const CFStringRef kIOSurfaceName                                     API_AVAILABLE(macos(11.0), ios(14.0), watchos(7.0), tvos(14.0));

typedef CF_ENUM(int32_t, IOSurfaceComponentName) {
    kIOSurfaceComponentNameUnknown      = 0,
    kIOSurfaceComponentNameAlpha        = 1,
    kIOSurfaceComponentNameRed          = 2,
    kIOSurfaceComponentNameGreen        = 3,
    kIOSurfaceComponentNameBlue         = 4,
    kIOSurfaceComponentNameLuma         = 5,
    kIOSurfaceComponentNameChromaRed    = 6,
    kIOSurfaceComponentNameChromaBlue   = 7,
} IOSFC_SWIFT_SENDABLE;

/* kIOSurfacePlaneComponentNames   - CFArray[CFNumber] for IOSurfaceComponentName of each component in this plane. 
    For example 'BGRA' would be {4, 3, 2, 1} */
extern const CFStringRef kIOSurfacePlaneComponentNames                      API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

typedef CF_ENUM(int32_t, IOSurfaceComponentType) {
    kIOSurfaceComponentTypeUnknown          = 0,
    kIOSurfaceComponentTypeUnsignedInteger  = 1,
    kIOSurfaceComponentTypeSignedInteger    = 2,
    kIOSurfaceComponentTypeFloat            = 3,
    kIOSurfaceComponentTypeSignedNormalized = 4,
} IOSFC_SWIFT_SENDABLE;

/* kIOSurfacePlaneComponentTypes   - CFArray[CFNumber] for IOSurfaceComponentType of each component in this plane. */
extern const CFStringRef kIOSurfacePlaneComponentTypes                      API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

typedef CF_ENUM(int32_t, IOSurfaceComponentRange) {
    kIOSurfaceComponentRangeUnknown     = 0,
    kIOSurfaceComponentRangeFullRange   = 1,
    kIOSurfaceComponentRangeVideoRange  = 2,
    kIOSurfaceComponentRangeWideRange   = 3,
} IOSFC_SWIFT_SENDABLE;

/* kIOSurfacePlaneComponentRanges   - CFArray[CFNumber] for IOSurfaceComponentRange of each component in this plane. */
extern const CFStringRef kIOSurfacePlaneComponentRanges                     API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

typedef CF_ENUM(int32_t, IOSurfaceSubsampling) {
    kIOSurfaceSubsamplingUnknown    = 0,
    kIOSurfaceSubsamplingNone       = 1, // Includes "4:4:4"
    kIOSurfaceSubsampling422        = 2, // Chroma downsampled by 2x1
    kIOSurfaceSubsampling420        = 3, // Chroma downsampled by 2x2
    kIOSurfaceSubsampling411        = 4, // Chroma downsampled by 4x1
} IOSFC_SWIFT_SENDABLE;

/* kIOSurfaceSubsampling   - CFNumber(IOSurfaceSubsampling) describing the chroma subsampling. */
extern const CFStringRef kIOSurfaceSubsampling                              API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

CFTypeID IOSurfaceGetTypeID(void)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Create a brand new IOSurface object */
IOSurfaceRef _Nullable IOSurfaceCreate(CFDictionaryRef properties)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Perform an atomic lookup and retain of a IOSurface by its IOSurfaceID.
   Note: Performing multiple lookups of the same IOSurface will *NOT* return
   the same IOSurfaceRef.   If you need to compare two IOSurface objects
   for equality, you must either do so by comparing their IOSurfaceIDs, or by 
   using CFEqual(). */
IOSurfaceRef _Nullable IOSurfaceLookup(IOSurfaceID csid) CF_RETURNS_RETAINED
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Retrieve the unique IOSurfaceID value for a IOSurface */
IOSurfaceID IOSurfaceGetID(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

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
    
    See the kIOSurfaceLock enums for more information.
    
    Note: Locking and unlocking a IOSurface is not a particularly cheap operation,
    so care should be taken to avoid the calls whenever possible.   The seed values are 
    particularly useful for keeping a cache of the buffer contents.
*/
kern_return_t IOSurfaceLock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t * _Nullable seed)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));
kern_return_t IOSurfaceUnlock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t * _Nullable seed)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* These routines are all fairly self explanatory.  0 is returned if buffer is invalid or NULL */
size_t IOSurfaceGetAllocSize(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetWidth(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetHeight(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetBytesPerElement(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetBytesPerRow(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

void *IOSurfaceGetBaseAddress(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetElementWidth(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetElementHeight(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

OSType IOSurfaceGetPixelFormat(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* This will return the current seed value of the buffer and is a cheap call to make to see
   if the contents of the buffer have changed since the last lock/unlock. */
uint32_t IOSurfaceGetSeed(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Return the number of planes in this buffer.  May be 0.   Returns 0 for an invalid or NULL buffer pointer. */
size_t IOSurfaceGetPlaneCount(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* These routines return information about a particular plane of a IOSurface.   

   If the planeIndex is greater than or equal to the plane count of the IOSurface, zero
   is returned.... with one exception.   If this IOSurface has zero planes and a planeIndex
   of zero is passed in, the routines function just like the non-planar APIs.  This is to allow
   higher level code to treat planar and non-planar buffers is a more uniform fashion. */

size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetElementWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetElementHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* These routines return information about a particular component of a particular plane of a IOSurface.
 For non-planar IOSurfaces, pass zero for planeIndex. */

size_t IOSurfaceGetNumberOfComponentsOfPlane(IOSurfaceRef buffer, size_t planeIndex)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

IOSurfaceComponentName IOSurfaceGetNameOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

IOSurfaceComponentType IOSurfaceGetTypeOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

IOSurfaceComponentRange IOSurfaceGetRangeOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetBitDepthOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

size_t IOSurfaceGetBitOffsetOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

IOSurfaceSubsampling IOSurfaceGetSubsampling(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.13), ios(11.0), watchos(4.0), tvos(11.0));

/* Common IOSurface keys for well-known IOSurface Values */

/* kIOSurfaceColorSpace defines the RGB (or other) color space for the data stored in the IOSurface. It
 is expected that this is a serialized CGColorSpaceRef, provided by the CoreGraphics CGColorSpaceRef
 serialization APIs (CGColorSpaceCopyPropertyList). */
extern const CFStringRef kIOSurfaceColorSpace API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* CFData representation of the ICC Profile, generally via CGColorSpaceCopyICCData */
extern const CFStringRef kIOSurfaceICCProfile  API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* CFNumber representation of the content headroom, which is defined as the ratio of nominal peak luminance
   ("peak white") to nominal diffuse luminance ("reference white" or "diffuse white"). */
extern const CFStringRef kIOSurfaceContentHeadroom API_AVAILABLE(macos(15.0), ios(18.0), watchos(11.0), tvos(18.0));

/* These  calls let you attach CF property list types to a IOSurface buffer.  These calls are
   expensive (they essentially must serialize the data into the kernel) and thus should be avoided whenever
   possible.   Note:  These functions can not be used to change the underlying surface properties. */
void IOSurfaceSetValue(IOSurfaceRef buffer, CFStringRef key, CFTypeRef value)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

CFTypeRef _Nullable IOSurfaceCopyValue(IOSurfaceRef buffer, CFStringRef key)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

void IOSurfaceRemoveValue(IOSurfaceRef buffer, CFStringRef key)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Bulk setters and getters for setting, retrieving or removing the entire
   set of values at once .*/
void IOSurfaceSetValues(IOSurfaceRef buffer, CFDictionaryRef keysAndValues)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

CFDictionaryRef _Nullable IOSurfaceCopyAllValues(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

void IOSurfaceRemoveAllValues(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* This call lets you get a mach_port_t that holds a reference to the IOSurface. This is useful 
   if you need to atomically or securely pass an IOSurface to another task without making the surface global to
   the entire system.  The returned port must be deallocated with mach_port_deallocate or the equivalent.  
   Note: Any live mach ports created from an IOSurfaceRef implicity increase the IOSurface's global use
   count by one until the port is deleted. */
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* This call lets you take a mach_port_t created via IOSurfaceCreatePort() and recreate an IOSurfaceRef from it.
   Note: This call does NOT destroy the port. */
IOSurfaceRef _Nullable IOSurfaceLookupFromMachPort(mach_port_t port) CF_RETURNS_RETAINED
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* 
   IOSurfaceGetPropertyMaximum() will return the maximum of a given property that is guaranteed to be 
   compatible with all of the current devices (GPUs, etc.) in the system.   The most important ones being:
   
   kIOSurfaceBytesPerRow
   kIOSurfaceWidth
   kIOSurfaceHeight
   kIOSurfacePlaneBytesPerRow
   kIOSurfacePlaneWidth
   kIOSurfacePlaneHeight
   
   For the width and height properties, the maximum values are the largest that are guaranteed to work
   for both reading and writing.   In OpenGL terms this translates into the largest size that will work
   for both textures and render targets.
   
   This function returns 0 for properties that have no predefined limit or where the concept of a limit
   would be considered invalid (such as kIOSurfacePixelFormat).
      
*/   
size_t IOSurfaceGetPropertyMaximum(CFStringRef property)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* 
   If a property has a particular alignment requirement, then IOSurfaceGetPropertyAlignment() will return it.  
   If the property has no alignment requirement then 1 will be returned.   The following properties 
   should always be aligned if you choose calculate them yourself:
   
   kIOSurfaceBytesPerRow
   kIOSurfaceOffset
   
   kIOSurfacePlaneBase
   kIOSurfacePlaneOffset
   kIOSurfacePlaneBytesPerRow
   
*/   
size_t IOSurfaceGetPropertyAlignment(CFStringRef property)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* This is a convenience function to automatically align property values.  For properties with no alignment
   requirements, the original value will be returned. */
size_t IOSurfaceAlignProperty(CFStringRef property, size_t value)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));


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
   calls or the process terminates), the global usage count is decremented by one.
   
   IOSurfaceGetUseCount() returns the local per-process usage count for an IOSurface.  This call is only
   provided for logging/debugging purposes and should never be used to determine whether an IOSurface is
   considered to be "in use".   IOSurfaceIsInUse() is the only call that should be used for that purpose. */
   
/* Increment the per-process usage count for an IOSurface */
void IOSurfaceIncrementUseCount(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Decrement the per-process usage count for an IOSurface */
void IOSurfaceDecrementUseCount(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Return the per-process usage count for an IOSurface */
int32_t IOSurfaceGetUseCount(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Returns true if an IOSurface is in use by any process in the system, otherwise false. */
Boolean IOSurfaceIsInUse(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.6), ios(11.0), watchos(4.0), tvos(11.0));

/* Returns true if this IOSurface allows pixel size casting */
Boolean IOSurfaceAllowsPixelSizeCasting(IOSurfaceRef buffer)
    API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

// This acts pretty much exactly like the Mach vm_purgeable object stuff does.
// Note: Higher level OpenGL and/or Metal based purgeability APIs should not be used for
// texture objects backed by IOSurfaces since they will essentially be ignored.

// You should assume that if you mark an IOSurface as Purgeable/Empty and mark it
// NonVolatile and it comes back with the old state being Empty that any texture
// objects bound to the IOSurface now have undefined content in them.
//
// If oldState is non-NULL, it will return the previous state of the IOSurface.
//
// kIOSurfacePurgeableNonVolatile - The IOSurface was not volatile and the contents are still valid
// kIOSurfacePurgeableVolatile    - The IOSurface was volatile, but the contents were not discarded
// kIOSurfacePurgeableEmpty       - The IOSurface was empty and the contents have been discarded.
// kIOSurfacePurgeableKeepCurrent - Don't change the current status, just return what the state is now.
kern_return_t IOSurfaceSetPurgeable(IOSurfaceRef buffer, uint32_t newState, uint32_t * __nullable oldState)
    API_AVAILABLE(macos(10.12), ios(11.0), watchos(4.0), tvos(11.0));

// Memory ledger tags.
typedef CF_ENUM(int, IOSurfaceMemoryLedgerTags) {
    kIOSurfaceMemoryLedgerTagDefault     = 0x00000001,
    kIOSurfaceMemoryLedgerTagNetwork     = 0x00000002,
    kIOSurfaceMemoryLedgerTagMedia       = 0x00000003,
    kIOSurfaceMemoryLedgerTagGraphics    = 0x00000004,
    kIOSurfaceMemoryLedgerTagNeural      = 0x00000005,
} IOSFC_SWIFT_SENDABLE;

// Memory ledger flags.
typedef CF_OPTIONS(uint32_t, IOSurfaceMemoryLedgerFlags) {
    kIOSurfaceMemoryLedgerFlagNoFootprint = (1 << 0),
} IOSFC_SWIFT_SENDABLE;


kern_return_t IOSurfaceSetOwnershipIdentity(IOSurfaceRef buffer, task_id_token_t task_id_token, int newLedgerTag, uint32_t newLedgerOptions) API_AVAILABLE(ios(17.4), watchos(10.4), tvos(17.4), macos(14.4));


__END_DECLS

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif
