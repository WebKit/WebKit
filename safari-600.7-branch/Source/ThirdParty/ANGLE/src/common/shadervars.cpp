//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// shadervars.cpp:
//   Implementation for GL shader variable member functions.
//

#include "common/shadervars.h"

namespace gl
{

ShaderVariable::ShaderVariable(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn)
    : type(typeIn),
      precision(precisionIn),
      name(nameIn),
      arraySize(arraySizeIn)
{
}

Uniform::Uniform(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, unsigned int registerIndexIn, unsigned int elementIndexIn)
    : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
      registerIndex(registerIndexIn),
      elementIndex(elementIndexIn)
{
}

Attribute::Attribute()
    : ShaderVariable(GL_NONE, GL_NONE, "", 0),
      location(-1)
{
}

Attribute::Attribute(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, int locationIn)
    : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
      location(locationIn)
{
}

InterfaceBlockField::InterfaceBlockField(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, bool isRowMajorMatrix)
    : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
      isRowMajorMatrix(isRowMajorMatrix)
{
}

Varying::Varying(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, InterpolationType interpolationIn)
    : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
      interpolation(interpolationIn),
      registerIndex(GL_INVALID_INDEX),
      elementIndex(GL_INVALID_INDEX)
{
}

void Varying::resetRegisterAssignment()
{
    registerIndex = GL_INVALID_INDEX;
    elementIndex = GL_INVALID_INDEX;
}

BlockMemberInfo::BlockMemberInfo(long offset, long arrayStride, long matrixStride, bool isRowMajorMatrix)
    : offset(offset),
      arrayStride(arrayStride),
      matrixStride(matrixStride),
      isRowMajorMatrix(isRowMajorMatrix)
{
}

const BlockMemberInfo BlockMemberInfo::defaultBlockInfo(-1, -1, -1, false);

InterfaceBlock::InterfaceBlock(const char *name, unsigned int arraySize, unsigned int registerIndex)
    : name(name),
      arraySize(arraySize),
      layout(BLOCKLAYOUT_SHARED),
      registerIndex(registerIndex),
      isRowMajorLayout(false)
{
}

}
