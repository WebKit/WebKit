//
//  MTLDefines.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <os/availability.h>
#import <TargetConditionals.h>

#define MTL_EXPORT __attribute__((visibility ("default")))
#define MTL_INTERN __attribute__((visibility ("hidden")))

#ifdef __cplusplus
#define MTL_EXTERN extern "C" MTL_EXPORT
#else
#define MTL_EXTERN extern MTL_EXPORT
#endif

#ifdef __cplusplus
#define MTL_EXTERN_NO_EXPORT extern "C" MTL_INTERN
#else
#define MTL_EXTERN_NO_EXPORT extern MTL_INTERN
#endif

/* Definition of 'MTL_INLINE'. */

#if !defined(MTL_INLINE)
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  define MTL_INLINE static inline
# elif defined(__cplusplus)
#  define MTL_INLINE static inline
# elif defined(__GNUC__)
#  define MTL_INLINE static __inline__
# else
#  define MTL_INLINE static
# endif
#endif

