//
// Copyright 2020 Apple, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EAGLFunctions.h: Exposing the soft-linked EAGL interface.

#ifndef EAGL_FUNCTIONS_H_
#define EAGL_FUNCTIONS_H_

#include "common/platform.h"

#if (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/EAGLIOSurface.h>

#include "libANGLE/renderer/gl/SoftLinking_apple.h"

SOFT_LINK_FRAMEWORK_HEADER(OpenGLES)

SOFT_LINK_CLASS_HEADER(EAGLContext)

#endif // (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

#endif // CGL_FUNCTIONS_H_
