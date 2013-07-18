//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// D3DConstantTable.h: Implements the D3DConstantTable class which parses
// information about constants from the CTAB comment in a D3D shader blob.
// Restructures the constant table as a hierarchy of constants in the same
// way as D3DX.

#ifndef LIBGLESV2_D3DCONSTANTTABLE_H_
#define LIBGLESV2_D3DCONSTANTTABLE_H_

#include <vector>
#include <string>

#include "common/angleutils.h"

namespace gl
{

namespace ctab
{
struct ConstantTable;
struct ConstantInfo;
struct TypeInfo;
struct StructMemberInfo;
}

struct D3DConstant
{
    // These enums match those in d3dx9shader.h.
    enum Class
    {
        CLASS_SCALAR,
        CLASS_VECTOR,
        CLASS_MATRIX_ROWS,
        CLASS_MATRIX_COLUMNS,
        CLASS_OBJECT,
        CLASS_STRUCT,
    };

    enum RegisterSet
    {
        RS_BOOL,
        RS_INT4,
        RS_FLOAT4,
        RS_SAMPLER,
    };

    enum Type
    {
        PT_VOID,
        PT_BOOL,
        PT_INT,
        PT_FLOAT,
        PT_STRING,
        PT_TEXTURE,
        PT_TEXTURE1D,
        PT_TEXTURE2D,
        PT_TEXTURE3D,
        PT_TEXTURECUBE,
        PT_SAMPLER,
        PT_SAMPLER1D,
        PT_SAMPLER2D,
        PT_SAMPLER3D,
        PT_SAMPLERCUBE,
        PT_PIXELSHADER,
        PT_VERTEXSHADER,
        PT_PIXELFRAGMENT,
        PT_VERTEXFRAGMENT,
        PT_UNSUPPORTED,
    };
    
    D3DConstant(const char *base, const ctab::ConstantInfo *constantInfo);
    ~D3DConstant();

    std::string name;
    RegisterSet registerSet;
    unsigned registerIndex;
    unsigned registerCount;
    Class typeClass;
    Type type;
    unsigned rows;
    unsigned columns;
    unsigned elements;

    // Array of structure members.
    std::vector<std::vector<const D3DConstant*> > structMembers;    

  private:
    D3DConstant(const char *base, RegisterSet registerSet, unsigned registerIndex, unsigned maxRegister, const ctab::StructMemberInfo *memberInfo);
    unsigned addStructMembers(const char *base, RegisterSet registerSet, unsigned registerIndex, unsigned maxRegister, const ctab::TypeInfo *typeInfo);
};

class D3DConstantTable
{
  public:
    D3DConstantTable(void *blob, size_t size);
    ~D3DConstantTable();

    bool error() const { return mError; }

    unsigned constants() const { return mConstants.size(); }
    const D3DConstant *getConstant(unsigned index) const;
    const D3DConstant *getConstantByName(const char *name) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(D3DConstantTable);
    std::vector<const D3DConstant*> mConstants;
    bool mError;
};

}

#endif   // LIBGLESV2_D3DCONSTANTTABLE_H_
