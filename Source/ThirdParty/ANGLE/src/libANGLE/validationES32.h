//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationES32.h:
//  Inlined validation functions for OpenGL ES 3.2 entry points.

#ifndef LIBANGLE_VALIDATION_ES32_H_
#define LIBANGLE_VALIDATION_ES32_H_

#include "libANGLE/ErrorStrings.h"
#include "libANGLE/validationES32_autogen.h"

namespace gl
{

void RecordVersionErrorES1Or32(const Context *context, angle::EntryPoint entryPoint);
void RecordVersionErrorES32(const Context *context, angle::EntryPoint entryPoint);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES32_H_
