//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationESEXT.h:
//  Inlined validation functions for OpenGL ES extension entry points.

#ifndef LIBANGLE_VALIDATION_ESEXT_H_
#define LIBANGLE_VALIDATION_ESEXT_H_

#include "libANGLE/ErrorStrings.h"
#include "libANGLE/validationESEXT_autogen.h"

namespace gl
{

void RecordVersionErrorESEXT(const Context *context, angle::EntryPoint entryPoint);

void RecordEntryPointBaseUnsupportedError(const Context *context, angle::EntryPoint entryPoint);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ESEXT_H_
