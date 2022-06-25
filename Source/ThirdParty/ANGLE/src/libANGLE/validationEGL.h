//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationEGL.h: Validation functions for generic EGL entry point parameters

#ifndef LIBANGLE_VALIDATIONEGL_H_
#define LIBANGLE_VALIDATIONEGL_H_

#include "common/PackedEnums.h"
#include "libANGLE/Error.h"
#include "libANGLE/Thread.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace gl
{
class Context;
}

namespace egl
{
constexpr EGLint kEglMajorVersion = 1;
constexpr EGLint kEglMinorVersion = 5;

class AttributeMap;
struct ClientExtensions;
struct Config;
class Device;
class Display;
class Image;
class Stream;
class Surface;
class Sync;
class Thread;
class LabeledObject;

struct ValidationContext
{
    ValidationContext(Thread *threadIn, const char *entryPointIn, const LabeledObject *objectIn)
        : eglThread(threadIn), entryPoint(entryPointIn), labeledObject(objectIn)
    {}

    // We should remove the message-less overload once we have messages for all EGL errors.
    void setError(EGLint error) const;
    ANGLE_FORMAT_PRINTF(3, 4)
    void setError(EGLint error, const char *message...) const;

    Thread *eglThread;
    const char *entryPoint;
    const LabeledObject *labeledObject;
};

// Object validation
bool ValidateDisplay(const ValidationContext *val, const Display *display);
bool ValidateSurface(const ValidationContext *val, const Display *display, const Surface *surface);
bool ValidateConfig(const ValidationContext *val, const Display *display, const Config *config);
bool ValidateContext(const ValidationContext *val,
                     const Display *display,
                     const gl::Context *context);
bool ValidateImage(const ValidationContext *val, const Display *display, const Image *image);
bool ValidateDevice(const ValidationContext *val, const Device *device);
bool ValidateSync(const ValidationContext *val, const Display *display, const Sync *sync);

// Return the requested object only if it is valid (otherwise nullptr)
const Thread *GetThreadIfValid(const Thread *thread);
const Display *GetDisplayIfValid(const Display *display);
const Surface *GetSurfaceIfValid(const Display *display, const Surface *surface);
const Image *GetImageIfValid(const Display *display, const Image *image);
const Stream *GetStreamIfValid(const Display *display, const Stream *stream);
const gl::Context *GetContextIfValid(const Display *display, const gl::Context *context);
const Device *GetDeviceIfValid(const Device *device);
const Sync *GetSyncIfValid(const Display *display, const Sync *sync);
LabeledObject *GetLabeledObjectIfValid(Thread *thread,
                                       const Display *display,
                                       ObjectType objectType,
                                       EGLObjectKHR object);

// A template struct for determining the default value to return for each entry point.
template <angle::EntryPoint EP, typename ReturnType>
struct DefaultReturnValue
{
    static constexpr ReturnType kValue = static_cast<ReturnType>(0);
};

template <angle::EntryPoint EP, typename ReturnType>
ReturnType GetDefaultReturnValue(Thread *thread);

template <>
ANGLE_INLINE EGLint
GetDefaultReturnValue<angle::EntryPoint::EGLLabelObjectKHR, EGLint>(Thread *thread)
{
    return thread->getError();
}

template <angle::EntryPoint EP, typename ReturnType>
ANGLE_INLINE ReturnType GetDefaultReturnValue(Thread *thread)
{
    return DefaultReturnValue<EP, ReturnType>::kValue;
}

// First case: handling packed enums.
template <typename PackedT, typename FromT>
typename std::enable_if<std::is_enum<PackedT>::value, PackedT>::type PackParam(FromT from)
{
    return FromEGLenum<PackedT>(from);
}

// This and the next 2 template specializations handle distinguishing between EGLint, EGLAttrib
// and other. This is needed because on some architectures EGLint and EGLAttrib are not the same
// base type. Previously the code conditionally compiled 2 specializations on 64 bit but it turns
// out on WatchOS the assumption about 32/64 bit and if EGLint and ELGAttrib are the same or
// different did not hold.
template <typename PackedT,
          typename FromT,
          typename std::enable_if<!std::is_enum<PackedT>::value>::type *              = nullptr,
          typename std::enable_if<std::is_same<FromT, const EGLint *>::value>::type * = nullptr>
typename std::remove_reference<PackedT>::type PackParam(FromT attribs)
{
    return AttributeMap::CreateFromIntArray(attribs);
}

template <typename PackedT,
          typename FromT,
          typename std::enable_if<!std::is_enum<PackedT>::value>::type *                 = nullptr,
          typename std::enable_if<!std::is_same<FromT, const EGLint *>::value>::type *   = nullptr,
          typename std::enable_if<std::is_same<FromT, const EGLAttrib *>::value>::type * = nullptr>
typename std::remove_reference<PackedT>::type PackParam(FromT attribs)
{
    return AttributeMap::CreateFromAttribArray(attribs);
}

template <typename PackedT,
          typename FromT,
          typename std::enable_if<!std::is_enum<PackedT>::value>::type *                  = nullptr,
          typename std::enable_if<!std::is_same<FromT, const EGLint *>::value>::type *    = nullptr,
          typename std::enable_if<!std::is_same<FromT, const EGLAttrib *>::value>::type * = nullptr>
typename std::remove_reference<PackedT>::type PackParam(FromT attribs)
{
    return static_cast<PackedT>(attribs);
}

}  // namespace egl

#define ANGLE_EGL_VALIDATE(THREAD, EP, OBJ, RETURN_TYPE, ...)                              \
    do                                                                                     \
    {                                                                                      \
        const char *epname = "egl" #EP;                                                    \
        ValidationContext vctx(THREAD, epname, OBJ);                                       \
        auto ANGLE_LOCAL_VAR = (Validate##EP(&vctx, ##__VA_ARGS__));                       \
        if (!ANGLE_LOCAL_VAR)                                                              \
        {                                                                                  \
            return GetDefaultReturnValue<angle::EntryPoint::EGL##EP, RETURN_TYPE>(THREAD); \
        }                                                                                  \
    } while (0)

#define ANGLE_EGL_VALIDATE_VOID(THREAD, EP, OBJ, ...)                \
    do                                                               \
    {                                                                \
        const char *epname = "egl" #EP;                              \
        ValidationContext vctx(THREAD, epname, OBJ);                 \
        auto ANGLE_LOCAL_VAR = (Validate##EP(&vctx, ##__VA_ARGS__)); \
        if (!ANGLE_LOCAL_VAR)                                        \
        {                                                            \
            return;                                                  \
        }                                                            \
    } while (0)

#define ANGLE_EGL_TRY(THREAD, EXPR, FUNCNAME, LABELOBJECT)                   \
    do                                                                       \
    {                                                                        \
        auto ANGLE_LOCAL_VAR = (EXPR);                                       \
        if (ANGLE_LOCAL_VAR.isError())                                       \
            return THREAD->setError(ANGLE_LOCAL_VAR, FUNCNAME, LABELOBJECT); \
    } while (0)

#define ANGLE_EGL_TRY_RETURN(THREAD, EXPR, FUNCNAME, LABELOBJECT, RETVAL) \
    do                                                                    \
    {                                                                     \
        auto ANGLE_LOCAL_VAR = (EXPR);                                    \
        if (ANGLE_LOCAL_VAR.isError())                                    \
        {                                                                 \
            THREAD->setError(ANGLE_LOCAL_VAR, FUNCNAME, LABELOBJECT);     \
            return RETVAL;                                                \
        }                                                                 \
    } while (0)

#define ANGLE_EGLBOOLEAN_TRY(EXPR)           \
    do                                       \
    {                                        \
        EGLBoolean ANGLE_LOCAL_VAR = (EXPR); \
        if (ANGLE_LOCAL_VAR != EGL_TRUE)     \
        {                                    \
            return ANGLE_LOCAL_VAR;          \
        }                                    \
    } while (0)

#endif  // LIBANGLE_VALIDATIONEGL_H_
