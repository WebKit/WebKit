//
// Copyright 2020 Apple, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EAGLFunctions.cpp: Exposing the soft-linked EAGL interface.

#include "common/platform.h"

#if (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/EAGLIOSurface.h>

#include "libANGLE/renderer/gl/SoftLinking_apple.h"

SOFT_LINK_FRAMEWORK_SOURCE(OpenGLES)

SOFT_LINK_CLASS(OpenGLES, EAGLContext)

#endif // (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

