//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// copyimage.h: Defines image copying functions

#ifndef LIBGLESV2_RENDERER_COPYIMAGE_H_
#define LIBGLESV2_RENDERER_COPYIMAGE_H_

#include "common/mathutil.h"
#include "libGLESv2/angletypes.h"

namespace rx
{

template <typename sourceType, typename colorDataType>
void ReadColor(const void *source, void *dest)
{
    sourceType::readColor(reinterpret_cast<gl::Color<colorDataType>*>(dest), reinterpret_cast<const sourceType*>(source));
}

template <typename destType, typename colorDataType>
void WriteColor(const void *source, void *dest)
{
    destType::writeColor(reinterpret_cast<destType*>(dest), reinterpret_cast<const gl::Color<colorDataType>*>(source));
}

template <typename sourceType, typename destType, typename colorDataType>
void CopyPixel(const void *source, void *dest)
{
    colorType temp;
    ReadColor<sourceType, colorDataType>(source, &temp);
    WriteColor<destType, colorDataType>(&temp, dest);
}

void CopyBGRAUByteToRGBAUByte(const void *source, void *dest);

}

#endif // LIBGLESV2_RENDERER_COPYIMAGE_H_
