/*
 *  IOSurfaceBase.h
 *  IOSurface
 *
 *  Copyright 2006-2017 Apple Inc. All rights reserved.
 *
 */

#ifndef IOSURFACE_BASE_H
#define IOSURFACE_BASE_H

#include <sys/cdefs.h>

#include <Availability.h>
#include <TargetConditionals.h>

#if defined(IOSFC_BUILDING_IOSFC)
#  define IOSFC_DEPRECATED_MSG(s)
#  define IOSFC_IOS_DEPRECATED_MSG(s)
#else /* !defined(IOSFC_BUILDING_IOSFC) */
#  define IOSFC_DEPRECATED_MSG(s) DEPRECATED_MSG_ATTRIBUTE(s)
#if TARGET_OS_IPHONE
#  define IOSFC_IOS_DEPRECATED_MSG(s) DEPRECATED_MSG_ATTRIBUTE(s)
#else
#  define IOSFC_IOS_DEPRECATED_MSG(s)
#endif
#endif /* !defined(IOSFC_BUILDING_IOSFC) */

#if __has_feature(objc_class_property)
#define IOSFC_SWIFT_NAME(name) __attribute__((swift_name(#name)))
#else
#define IOSFC_SWIFT_NAME(name)
#endif

#if defined(__SWIFT_ATTR_SUPPORTS_SENDABLE_DECLS) && __SWIFT_ATTR_SUPPORTS_SENDABLE_DECLS
    // The typedef or struct should be imported as 'Sendable' in Swift
    #define IOSFC_SWIFT_SENDABLE __attribute__((swift_attr("@Sendable")))
    // The typedef or struct should *not* be imported as 'Sendable' in Swift even if it normally would be
    #define IOSFC_SWIFT_NONSENDABLE __attribute__((swift_attr("@_nonSendable")))
#else
    #define IOSFC_SWIFT_SENDABLE
    #define IOSFC_SWIFT_NONSENDABLE
#endif // __SWIFT_ATTR_SUPPORTS_SENDABLE_DECLS

#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <CoreFoundation/CoreFoundation.h>
#if TARGET_OS_OSX
#include <xpc/xpc.h>
#endif

#endif
