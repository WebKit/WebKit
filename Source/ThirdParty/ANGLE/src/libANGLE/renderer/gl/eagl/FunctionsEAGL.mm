//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsEAGL.cpp: Exposing the soft-linked EAGL interface.

#include "common/platform.h"

#if defined(ANGLE_ENABLE_EAGL)

// OpenGL ES is technically deprecated on iOS. Silence associated warnings.
#    define GLES_SILENCE_DEPRECATION

#    import <OpenGLES/EAGL.h>
#    import <OpenGLES/EAGLDrawable.h>
#    import <OpenGLES/EAGLIOSurface.h>

#    include "common/apple/SoftLinking.h"

SOFT_LINK_FRAMEWORK_SOURCE(OpenGLES)

SOFT_LINK_CLASS(OpenGLES, EAGLContext)

#endif  // defined(ANGLE_ENABLE_EAGL)
