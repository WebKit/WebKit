/*
 *  IOSurfaceTypes.h
 *  IOSurface
 *
 *  Copyright 2006-2017 Apple Inc. All rights reserved.
 *
 */

#ifndef IOSURFACE_TYPES_H
#define IOSURFACE_TYPES_H

#include <IOSurface/IOSurfaceBase.h>

typedef uint32_t IOSurfaceID;

typedef CF_OPTIONS(uint32_t, IOSurfaceLockOptions)
{
    // If you are not going to modify the data while you hold the lock, you should set this flag to avoid invalidating
    // any existing caches of the buffer contents.  This flag should be passed both to the lock and unlock functions.
    // Non-symmetrical usage of this flag will result in undefined behavior.
    kIOSurfaceLockReadOnly  =   0x00000001,
    
    // If you want to detect/avoid a potentially expensive paging operation (such as readback from a GPU to system memory)
    // when you lock the buffer, you may include this flag.   If locking the buffer requires a readback, the lock will
    // fail with an error return of kIOReturnCannotLock.
    kIOSurfaceLockAvoidSync =   0x00000002,
} IOSFC_SWIFT_SENDABLE;

typedef CF_OPTIONS(uint32_t, IOSurfacePurgeabilityState)
{
    kIOSurfacePurgeableNonVolatile = 0,   // Mark the IOSurface as non-volatile
    kIOSurfacePurgeableVolatile    = 1,   // Mark the IOSurface as volatile (contents may be thrown away)
    kIOSurfacePurgeableEmpty       = 2,   // Throw away the contents of the IOSurface immediately
    kIOSurfacePurgeableKeepCurrent = 3,   // Keep the current setting (useful for returning current status info)
} IOSFC_SWIFT_SENDABLE;

/*
** Note: Write-combined memory is optimized for resources that the CPU writes into, but never reads. 
** On some implementations, writes may bypass caches, which avoids cache pollution. Read actions may perform very poorly.
** Applications should investigate changing the cache mode only if writing to normally cached buffers is known to cause
** performance issues due to cache pollution, as write-combined memory can have surprising performance pitfalls.
*/
enum {
    kIOSurfaceDefaultCache              = 0,
    kIOSurfaceInhibitCache              = 1,
    kIOSurfaceWriteThruCache            = 2,
    kIOSurfaceCopybackCache             = 3,
    kIOSurfaceWriteCombineCache         = 4,
    kIOSurfaceCopybackInnerCache        = 5
};

// IOSurface Memory mapping options (used with kIOSurfaceCacheMode or IOSurfacePropertyKeyCacheMode)
enum {
    kIOSurfaceMapCacheShift             = 8,
    kIOSurfaceMapDefaultCache           = kIOSurfaceDefaultCache       << kIOSurfaceMapCacheShift,
    kIOSurfaceMapInhibitCache           = kIOSurfaceInhibitCache       << kIOSurfaceMapCacheShift,
    kIOSurfaceMapWriteThruCache         = kIOSurfaceWriteThruCache     << kIOSurfaceMapCacheShift,
    kIOSurfaceMapCopybackCache          = kIOSurfaceCopybackCache      << kIOSurfaceMapCacheShift,
    kIOSurfaceMapWriteCombineCache      = kIOSurfaceWriteCombineCache  << kIOSurfaceMapCacheShift,
    kIOSurfaceMapCopybackInnerCache     = kIOSurfaceCopybackInnerCache << kIOSurfaceMapCacheShift,
};

/* IOSurface APIs that return a kern_return_t will return either kIOSurfaceSuccess or an internal error code for failures */
#define kIOSurfaceSuccess         KERN_SUCCESS            // OK

#endif
