/*
 * Copyright 2024 Igalia S.L.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/gpu/gl/GrGLInterface.h"
#include "include/gpu/gl/epoxy/GrGLMakeEpoxyEGLInterface.h"

sk_sp<const GrGLInterface> GrGLMakeNativeInterface() { return GrGLMakeEpoxyEGLInterface(); }

