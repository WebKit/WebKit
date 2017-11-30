//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// params:
//   Parameter wrapper structs for OpenGL ES. These helpers cache re-used values
//   in entry point routines.

#ifndef LIBANGLE_PARAMS_H_
#define LIBANGLE_PARAMS_H_

#include "angle_gl.h"
#include "common/Optional.h"
#include "common/angleutils.h"
#include "common/mathutil.h"
#include "libANGLE/entry_points_enum_autogen.h"

namespace gl
{
class Context;

template <EntryPoint EP>
struct EntryPointParam;

template <EntryPoint EP>
using EntryPointParamType = typename EntryPointParam<EP>::Type;

class ParamTypeInfo
{
  public:
    constexpr ParamTypeInfo(const char *selfClass, const ParamTypeInfo *parentType)
        : mSelfClass(selfClass), mParentTypeInfo(parentType)
    {
    }

    constexpr bool hasDynamicType(const ParamTypeInfo &typeInfo) const
    {
        return mSelfClass == typeInfo.mSelfClass ||
               (mParentTypeInfo && mParentTypeInfo->hasDynamicType(typeInfo));
    }

    constexpr bool isValid() const { return mSelfClass != nullptr; }

  private:
    const char *mSelfClass;
    const ParamTypeInfo *mParentTypeInfo;
};

#define ANGLE_PARAM_TYPE_INFO(NAME, BASENAME) \
    static constexpr ParamTypeInfo TypeInfo = {#NAME, &BASENAME::TypeInfo}

class ParamsBase : angle::NonCopyable
{
  public:
    ParamsBase(Context *context, ...){};

    template <EntryPoint EP, typename... ArgsT>
    static void Factory(EntryPointParamType<EP> *objBuffer, ArgsT... args);

    static constexpr ParamTypeInfo TypeInfo = {nullptr, nullptr};
};

// static
template <EntryPoint EP, typename... ArgsT>
ANGLE_INLINE void ParamsBase::Factory(EntryPointParamType<EP> *objBuffer, ArgsT... args)
{
    new (objBuffer) EntryPointParamType<EP>(args...);
}

class HasIndexRange : public ParamsBase
{
  public:
    // Dummy placeholder that can't generate an index range.
    HasIndexRange();
    HasIndexRange(Context *context, GLsizei count, GLenum type, const void *indices);

    template <EntryPoint EP, typename... ArgsT>
    static void Factory(HasIndexRange *objBuffer, ArgsT... args);

    const Optional<IndexRange> &getIndexRange() const;

    ANGLE_PARAM_TYPE_INFO(HasIndexRange, ParamsBase);

  private:
    Context *mContext;
    GLsizei mCount;
    GLenum mType;
    const GLvoid *mIndices;
    mutable Optional<IndexRange> mIndexRange;
};

// Entry point funcs essentially re-map different entry point parameter arrays into
// the format the parameter type class expects. For example, for HasIndexRange, for the
// various indexed draw calls, they drop parameters that aren't useful and re-arrange
// the rest.
#define ANGLE_ENTRY_POINT_FUNC(NAME, CLASS, ...)    \
    \
template<> struct EntryPointParam<EntryPoint::NAME> \
    {                                               \
        using Type = CLASS;                         \
    };                                              \
    \
template<> inline void CLASS::Factory<EntryPoint::NAME>(__VA_ARGS__)

ANGLE_ENTRY_POINT_FUNC(DrawElements,
                       HasIndexRange,
                       HasIndexRange *objBuffer,
                       Context *context,
                       GLenum /*mode*/,
                       GLsizei count,
                       GLenum type,
                       const void *indices)
{
    return ParamsBase::Factory<EntryPoint::DrawElements>(objBuffer, context, count, type, indices);
}

ANGLE_ENTRY_POINT_FUNC(DrawElementsInstanced,
                       HasIndexRange,
                       HasIndexRange *objBuffer,
                       Context *context,
                       GLenum /*mode*/,
                       GLsizei count,
                       GLenum type,
                       const void *indices,
                       GLsizei /*instanceCount*/)
{
    return ParamsBase::Factory<EntryPoint::DrawElementsInstanced>(objBuffer, context, count, type,
                                                                  indices);
}

ANGLE_ENTRY_POINT_FUNC(DrawElementsInstancedANGLE,
                       HasIndexRange,
                       HasIndexRange *objBuffer,
                       Context *context,
                       GLenum /*mode*/,
                       GLsizei count,
                       GLenum type,
                       const void *indices,
                       GLsizei /*instanceCount*/)
{
    return ParamsBase::Factory<EntryPoint::DrawElementsInstancedANGLE>(objBuffer, context, count,
                                                                       type, indices);
}

ANGLE_ENTRY_POINT_FUNC(DrawRangeElements,
                       HasIndexRange,
                       HasIndexRange *objBuffer,
                       Context *context,
                       GLenum /*mode*/,
                       GLuint /*start*/,
                       GLuint /*end*/,
                       GLsizei count,
                       GLenum type,
                       const void *indices)
{
    return ParamsBase::Factory<EntryPoint::DrawRangeElements>(objBuffer, context, count, type,
                                                              indices);
}

#undef ANGLE_ENTRY_POINT_FUNC

template <EntryPoint EP>
struct EntryPointParam
{
    using Type = ParamsBase;
};

// A template struct for determining the default value to return for each entry point.
template <EntryPoint EP, typename ReturnType>
struct DefaultReturnValue;

// Default return values for each basic return type.
template <EntryPoint EP>
struct DefaultReturnValue<EP, GLint>
{
    static constexpr GLint kValue = -1;
};

// This doubles as the GLenum return value.
template <EntryPoint EP>
struct DefaultReturnValue<EP, GLuint>
{
    static constexpr GLuint kValue = 0;
};

template <EntryPoint EP>
struct DefaultReturnValue<EP, GLboolean>
{
    static constexpr GLboolean kValue = GL_FALSE;
};

// Catch-all rules for pointer types.
template <EntryPoint EP, typename PointerType>
struct DefaultReturnValue<EP, const PointerType *>
{
    static constexpr const PointerType *kValue = nullptr;
};

template <EntryPoint EP, typename PointerType>
struct DefaultReturnValue<EP, PointerType *>
{
    static constexpr PointerType *kValue = nullptr;
};

// Overloaded to return invalid index
template <>
struct DefaultReturnValue<EntryPoint::GetUniformBlockIndex, GLuint>
{
    static constexpr GLuint kValue = GL_INVALID_INDEX;
};

// Specialized enum error value.
template <>
struct DefaultReturnValue<EntryPoint::ClientWaitSync, GLenum>
{
    static constexpr GLenum kValue = GL_WAIT_FAILED;
};

template <EntryPoint EP, typename ReturnType>
constexpr ANGLE_INLINE ReturnType GetDefaultReturnValue()
{
    return DefaultReturnValue<EP, ReturnType>::kValue;
}

}  // namespace gl

#endif  // LIBANGLE_PARAMS_H_
