//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// D3DConstantTable.cpp: Implements the D3DConstantTable class which parses
// information about constants from the CTAB comment in a D3D shader blob.
// Restructures the constant table as a hierarchy of constants in the same
// way as D3DX.

#include "libGLESv2/D3DConstantTable.h"

#include "common/system.h"
#include <d3d9.h>
#include <d3d9types.h>
#include <mmsystem.h>

#include "libGLESv2/BinaryStream.h"

const static int SHADER_VERSION_MASK = D3DVS_VERSION(0, 0);
const static int FOURCC_CTAB = MAKEFOURCC('C','T','A','B');

namespace gl
{
// These structs and constants correspond to the format of the constant table in a shader binary.
// They match the corresponding structures in d3dx9shader.h.
namespace ctab
{
struct ConstantTable
{
    DWORD size;
    DWORD creator;
    DWORD version;
    DWORD constants;
    DWORD constantInfos;
    DWORD flags;
    DWORD target;
};

struct ConstantInfo
{
    DWORD name;
    WORD registerSet;
    WORD registerIndex;
    WORD registerCount;
    WORD reserved;
    DWORD typeInfo;
    DWORD defaultValue;
};

struct TypeInfo
{
    WORD typeClass;
    WORD type;
    WORD rows;
    WORD columns;
    WORD elements;
    WORD structMembers;
    DWORD structMemberInfos;
};

struct StructMemberInfo
{
    DWORD name;
    DWORD typeInfo;
};
}

D3DConstant::D3DConstant(const char *base, const ctab::ConstantInfo *constantInfo)
{
    const ctab::TypeInfo *typeInfo = reinterpret_cast<const ctab::TypeInfo*>(base + constantInfo->typeInfo);

    name = base + constantInfo->name;
    registerSet = static_cast<RegisterSet>(constantInfo->registerSet);
    registerIndex = constantInfo->registerIndex;
    registerCount = constantInfo->registerCount;
    typeClass = static_cast<Class>(typeInfo->typeClass);
    type = static_cast<Type>(typeInfo->type);
    rows = typeInfo->rows;
    columns = typeInfo->columns;
    elements = typeInfo->elements;

    if (typeClass == CLASS_STRUCT)
    {
        addStructMembers(base, registerSet, registerIndex, registerIndex + registerCount, typeInfo);
    }
}

D3DConstant::D3DConstant(const char *base, RegisterSet registerSet, unsigned registerIndex, unsigned maxRegister, const ctab::StructMemberInfo *memberInfo)
    : registerSet(registerSet), registerIndex(registerIndex)
{
    const ctab::TypeInfo *typeInfo = reinterpret_cast<const ctab::TypeInfo*>(base + memberInfo->typeInfo);

    name = base + memberInfo->name;
    registerCount = std::min(static_cast<int>(maxRegister - registerIndex), typeInfo->rows * typeInfo->elements);
    typeClass = static_cast<Class>(typeInfo->typeClass);
    type = static_cast<Type>(typeInfo->type);
    rows = typeInfo->rows;
    columns = typeInfo->columns;
    elements = typeInfo->elements;

    if (typeClass == CLASS_STRUCT)
    {
        registerCount = addStructMembers(base, registerSet, registerIndex, maxRegister, typeInfo);
    }
}

D3DConstant::~D3DConstant()
{
    for (size_t j = 0; j < structMembers.size(); ++j)
    {
        for (size_t i = 0; i < structMembers[j].size(); ++i)
        {
            delete structMembers[j][i];
        }
    }
}

unsigned D3DConstant::addStructMembers(const char *base, RegisterSet registerSet, unsigned registerIndex, unsigned maxRegister, const ctab::TypeInfo *typeInfo)
{
    const ctab::StructMemberInfo *memberInfos = reinterpret_cast<const ctab::StructMemberInfo*>(
        base + typeInfo->structMemberInfos);

    unsigned memberIndex = registerIndex;

    structMembers.resize(elements);

    for (unsigned j = 0; j < elements; ++j)
    {
        structMembers[j].resize(typeInfo->structMembers);

        for (unsigned i = 0; i < typeInfo->structMembers; ++i)
        {
            const ctab::TypeInfo *memberTypeInfo = reinterpret_cast<const ctab::TypeInfo*>(
                base + memberInfos[i].typeInfo);

            D3DConstant *member = new D3DConstant(base, registerSet, memberIndex, maxRegister, memberInfos + i);
            memberIndex += member->registerCount;

            structMembers[j][i] = member;
        }
    }

    return memberIndex - registerIndex;
}

D3DConstantTable::D3DConstantTable(void *blob, size_t size) : mError(false)
{
    BinaryInputStream stream(blob, size);

    int version;
    stream.read(&version);
    if ((version & SHADER_VERSION_MASK) != SHADER_VERSION_MASK)
    {
        mError = true;
        return;
    }

    const ctab::ConstantTable* constantTable = NULL;

    while (!stream.error())
    {
        int token;
        stream.read(&token);

        if ((token & D3DSI_OPCODE_MASK) == D3DSIO_COMMENT)
        {
            size_t length = ((token & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT) * sizeof(DWORD);

            int fourcc;
            stream.read(&fourcc);
            if (fourcc == FOURCC_CTAB)
            {
                constantTable = reinterpret_cast<const ctab::ConstantTable*>(static_cast<const char*>(blob) + stream.offset());
                break;
            }

            stream.skip(length - sizeof(fourcc));
        }
        else if (token == D3DSIO_END)
        {
            break;
        }
    }

    mError = !constantTable || stream.error();
    if (mError)
    {
        return;
    }

    const char *base = reinterpret_cast<const char*>(constantTable);

    mConstants.resize(constantTable->constants);
    const ctab::ConstantInfo *constantInfos =
        reinterpret_cast<const ctab::ConstantInfo*>(base + constantTable->constantInfos);
    for (size_t i = 0; i < constantTable->constants; ++i)
    {
        mConstants[i] = new D3DConstant(base, constantInfos + i);
    }
}

D3DConstantTable::~D3DConstantTable()
{
    for (size_t i = 0; i < mConstants.size(); ++i)
    {
        delete mConstants[i];
    }
}

const D3DConstant *D3DConstantTable::getConstant(unsigned index) const
{
    return mConstants[index];
}

const D3DConstant *D3DConstantTable::getConstantByName(const char *name) const
{
    for (size_t i = 0; i < mConstants.size(); ++i)
    {
        const D3DConstant *constant = getConstant(i);
        if (constant->name == name)
        {
            return constant;
        }
    }

    return NULL;
}

}
