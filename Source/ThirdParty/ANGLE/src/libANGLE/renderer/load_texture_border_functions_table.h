//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// load_texture_border_functions_table:
//   Contains load texture border functions table depending on internal format and ANGLE format.
//

#ifndef LIBANGLE_RENDERER_LOADTEXTUREBORDERFUNCTIONSTABLE_H_
#define LIBANGLE_RENDERER_LOADTEXTUREBORDERFUNCTIONSTABLE_H_

#include "libANGLE/renderer/Format.h"

namespace angle
{
rx::LoadTextureBorderFunctionMap GetLoadTextureBorderFunctionsMap(GLenum internalFormat,
                                                                  FormatID angleFormat);
}  // namespace angle

#endif  // LIBANGLE_RENDERER_LOADTEXTUREBORDERFUNCTIONSTABLE_H_
