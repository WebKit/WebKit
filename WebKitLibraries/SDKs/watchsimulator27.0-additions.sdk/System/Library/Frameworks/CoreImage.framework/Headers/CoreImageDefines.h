/* CoreImage - CoreImageDefines.h
 
 Copyright (c) 2014 Apple, Inc.
 All rights reserved. */

#ifndef COREIMAGEDEFINES_H
#define COREIMAGEDEFINES_H

#ifndef __METAL_VERSION__

#include <TargetConditionals.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <CoreGraphics/CoreGraphics.h>
#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

#ifdef __cplusplus
 #define CI_EXTERN_C_BEGIN  extern "C" {
 #define CI_EXTERN_C_END  }
#else
 #define CI_EXTERN_C_BEGIN
 #define CI_EXTERN_C_END
#endif

#ifdef __cplusplus
# define CORE_IMAGE_EXPORT extern "C" __attribute__((visibility("default")))
#else
# define CORE_IMAGE_EXPORT extern __attribute__((visibility("default")))
#endif

#define CORE_IMAGE_CLASS_EXPORT __attribute__((visibility("default")))


#ifdef __OBJC__
#ifdef CI_SILENCE_GL_DEPRECATION
  #if defined(TARGET_OS_VISION) && TARGET_OS_VISION
  #define CI_GL_DEPRECATED(...)  API_UNAVAILABLE(visionos)
  #define CI_GL_DEPRECATED_IOS(...) API_UNAVAILABLE(visionos)
  #else
  #define CI_GL_DEPRECATED(fromM,toM, fromI,toI)  NS_AVAILABLE(fromM,fromI)
  #define CI_GL_DEPRECATED_IOS(from, to)  NS_AVAILABLE_IOS(from)
  #endif
  #define CI_GL_DEPRECATED_MAC(from, to)  NS_AVAILABLE_MAC(from)
  #define CIKL_DEPRECATED(fromM,toM, fromI,toI)  NS_AVAILABLE(fromM,fromI)
#else
  #if defined(TARGET_OS_VISION) && TARGET_OS_VISION
  #define CI_GL_DEPRECATED(...)  API_UNAVAILABLE(visionos)
  #define CI_GL_DEPRECATED_IOS(...) API_UNAVAILABLE(visionos)
  #else
  #define CI_GL_DEPRECATED(fromM,toM, fromI,toI)  NS_DEPRECATED(fromM,toM, fromI,toI, "Core Image OpenGL API deprecated. (Define CI_SILENCE_GL_DEPRECATION to silence these warnings)")
  #define CI_GL_DEPRECATED_IOS(from, to)  NS_DEPRECATED_IOS(from, to, "Core Image OpenGLES API deprecated. (Define CI_SILENCE_GL_DEPRECATION to silence these warnings)")
  #endif
  #define CI_GL_DEPRECATED_MAC(from, to)  NS_DEPRECATED_MAC(from, to, "Core Image OpenGL API deprecated. (Define CI_SILENCE_GL_DEPRECATION to silence these warnings)")
  #define CIKL_DEPRECATED(fromM,toM, fromI,toI)  NS_DEPRECATED(fromM,toM, fromI,toI, "Core Image Kernel Language API deprecated. (Define CI_SILENCE_GL_DEPRECATION to silence these warnings)")
#endif
#endif

#define COREIMAGE_SUPPORTS_IOSURFACE 1

#define UNIFIED_CORE_IMAGE 1

#endif /* !__METAL_VERSION__ */

#endif /* COREIMAGEDEFINES_H */
