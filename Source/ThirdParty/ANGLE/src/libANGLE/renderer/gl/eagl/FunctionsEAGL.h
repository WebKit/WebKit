//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsEAGL.h: Exposing the soft-linked EAGL interface.

#ifndef EAGL_FUNCTIONS_H_
#define EAGL_FUNCTIONS_H_

#include <OpenGLES/EAGL.h>
#include <OpenGLES/EAGLDrawable.h>
#include <OpenGLES/EAGLIOSurface.h>

#include "common/apple/SoftLinking.h"

SOFT_LINK_FRAMEWORK_HEADER(OpenGLES)

SOFT_LINK_CLASS_HEADER(EAGLContext)

#endif  // EAGL_FUNCTIONS_H_
