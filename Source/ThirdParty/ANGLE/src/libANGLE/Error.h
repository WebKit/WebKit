//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Error.h: Defines the egl::Error and gl::Error classes which encapsulate API errors
// and optional error messages.

#ifndef LIBANGLE_ERROR_H_
#define LIBANGLE_ERROR_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "angle_gl.h"
#include "common/angleutils.h"
#include "common/debug.h"

#include <memory>
#include <ostream>
#include <string>

namespace angle
{
template <typename ErrorT, typename ResultT, typename ErrorBaseT, ErrorBaseT NoErrorVal>
class ANGLE_NO_DISCARD ErrorOrResultBase
{
  public:
    ErrorOrResultBase(const ErrorT &error) : mError(error) {}
    ErrorOrResultBase(ErrorT &&error) : mError(std::move(error)) {}

    ErrorOrResultBase(ResultT &&result) : mError(NoErrorVal), mResult(std::forward<ResultT>(result))
    {
    }

    ErrorOrResultBase(const ResultT &result) : mError(NoErrorVal), mResult(result) {}

    bool isError() const { return mError.isError(); }
    const ErrorT &getError() const { return mError; }
    ResultT &&getResult() { return std::move(mResult); }

  private:
    ErrorT mError;
    ResultT mResult;
};

template <typename ErrorT, typename ErrorBaseT, ErrorBaseT NoErrorVal, typename CodeT, CodeT EnumT>
class ErrorStreamBase : angle::NonCopyable
{
  public:
    ErrorStreamBase() : mID(EnumT) {}
    ErrorStreamBase(GLuint id) : mID(id) {}

    template <typename T>
    ErrorStreamBase &operator<<(T value)
    {
        mErrorStream << value;
        return *this;
    }

    operator ErrorT() { return ErrorT(EnumT, mID, mErrorStream.str()); }

    template <typename ResultT>
    operator ErrorOrResultBase<ErrorT, ResultT, ErrorBaseT, NoErrorVal>()
    {
        return static_cast<ErrorT>(*this);
    }

  private:
    GLuint mID;
    std::ostringstream mErrorStream;
};
}  // namespace angle

namespace egl
{
class Error;
}  // namespace egl

namespace gl
{

class ANGLE_NO_DISCARD Error final
{
  public:
    explicit inline Error(GLenum errorCode);
    Error(GLenum errorCode, std::string &&message);
    Error(GLenum errorCode, GLuint id, std::string &&message);
    inline Error(const Error &other);
    inline Error(Error &&other);
    inline ~Error() = default;

    // automatic error type conversion
    inline Error(egl::Error &&eglErr);
    inline Error(egl::Error eglErr);

    inline Error &operator=(const Error &other);
    inline Error &operator=(Error &&other);

    inline GLenum getCode() const;
    inline GLuint getID() const;
    inline bool isError() const;

    const std::string &getMessage() const;

    // Useful for mocking and testing
    bool operator==(const Error &other) const;
    bool operator!=(const Error &other) const;

  private:
    void createMessageString() const;

    friend std::ostream &operator<<(std::ostream &os, const Error &err);
    friend class egl::Error;

    GLenum mCode;
    GLuint mID;
    mutable std::unique_ptr<std::string> mMessage;
};

template <typename ResultT>
using ErrorOrResult = angle::ErrorOrResultBase<Error, ResultT, GLenum, GL_NO_ERROR>;

namespace priv
{

template <GLenum EnumT>
using ErrorStream = angle::ErrorStreamBase<Error, GLenum, GL_NO_ERROR, GLenum, EnumT>;

}  // namespace priv

using InternalError = priv::ErrorStream<GL_INVALID_OPERATION>;

using InvalidEnum                 = priv::ErrorStream<GL_INVALID_ENUM>;
using InvalidValue                = priv::ErrorStream<GL_INVALID_VALUE>;
using InvalidOperation            = priv::ErrorStream<GL_INVALID_OPERATION>;
using StackOverflow               = priv::ErrorStream<GL_STACK_OVERFLOW>;
using StackUnderflow              = priv::ErrorStream<GL_STACK_UNDERFLOW>;
using OutOfMemory                 = priv::ErrorStream<GL_OUT_OF_MEMORY>;
using InvalidFramebufferOperation = priv::ErrorStream<GL_INVALID_FRAMEBUFFER_OPERATION>;

inline Error NoError()
{
    return Error(GL_NO_ERROR);
}

using LinkResult = ErrorOrResult<bool>;

}  // namespace gl

namespace egl
{

class ANGLE_NO_DISCARD Error final
{
  public:
    explicit inline Error(EGLint errorCode);
    Error(EGLint errorCode, std::string &&message);
    Error(EGLint errorCode, EGLint id, std::string &&message);
    inline Error(const Error &other);
    inline Error(Error &&other);
    inline ~Error() = default;

    // automatic error type conversion
    inline Error(gl::Error &&glErr);
    inline Error(gl::Error glErr);

    inline Error &operator=(const Error &other);
    inline Error &operator=(Error &&other);

    inline EGLint getCode() const;
    inline EGLint getID() const;
    inline bool isError() const;

    const std::string &getMessage() const;

  private:
    void createMessageString() const;

    friend std::ostream &operator<<(std::ostream &os, const Error &err);
    friend class gl::Error;

    EGLint mCode;
    EGLint mID;
    mutable std::unique_ptr<std::string> mMessage;
};

template <typename ResultT>
using ErrorOrResult = angle::ErrorOrResultBase<Error, ResultT, EGLint, EGL_SUCCESS>;

namespace priv
{

template <EGLint EnumT>
using ErrorStream = angle::ErrorStreamBase<Error, EGLint, EGL_SUCCESS, EGLint, EnumT>;

}  // namespace priv

using EglNotInitialized    = priv::ErrorStream<EGL_NOT_INITIALIZED>;
using EglBadAccess         = priv::ErrorStream<EGL_BAD_ACCESS>;
using EglBadAlloc          = priv::ErrorStream<EGL_BAD_ALLOC>;
using EglBadAttribute      = priv::ErrorStream<EGL_BAD_ATTRIBUTE>;
using EglBadConfig         = priv::ErrorStream<EGL_BAD_CONFIG>;
using EglBadContext        = priv::ErrorStream<EGL_BAD_CONTEXT>;
using EglBadCurrentSurface = priv::ErrorStream<EGL_BAD_CURRENT_SURFACE>;
using EglBadDisplay        = priv::ErrorStream<EGL_BAD_DISPLAY>;
using EglBadMatch          = priv::ErrorStream<EGL_BAD_MATCH>;
using EglBadNativeWindow   = priv::ErrorStream<EGL_BAD_NATIVE_WINDOW>;
using EglBadParameter      = priv::ErrorStream<EGL_BAD_PARAMETER>;
using EglBadSurface        = priv::ErrorStream<EGL_BAD_SURFACE>;
using EglContextLost       = priv::ErrorStream<EGL_CONTEXT_LOST>;
using EglBadStream         = priv::ErrorStream<EGL_BAD_STREAM_KHR>;
using EglBadState          = priv::ErrorStream<EGL_BAD_STATE_KHR>;
using EglBadDevice         = priv::ErrorStream<EGL_BAD_DEVICE_EXT>;

inline Error NoError()
{
    return Error(EGL_SUCCESS);
}

}  // namespace egl

#define ANGLE_CONCAT1(x, y) x##y
#define ANGLE_CONCAT2(x, y) ANGLE_CONCAT1(x, y)
#define ANGLE_LOCAL_VAR ANGLE_CONCAT2(_localVar, __LINE__)

#define ANGLE_TRY_TEMPLATE(EXPR, FUNC) \
    {                                  \
        auto ANGLE_LOCAL_VAR = EXPR;   \
        if (ANGLE_LOCAL_VAR.isError()) \
        {                              \
            FUNC(ANGLE_LOCAL_VAR);     \
        }                              \
    }                                  \
    ANGLE_EMPTY_STATEMENT

#define ANGLE_RETURN(X) return X;
#define ANGLE_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_RETURN);

#define ANGLE_TRY_RESULT(EXPR, RESULT)         \
    {                                          \
        auto ANGLE_LOCAL_VAR = EXPR;           \
        if (ANGLE_LOCAL_VAR.isError())         \
        {                                      \
            return ANGLE_LOCAL_VAR.getError(); \
        }                                      \
        RESULT = ANGLE_LOCAL_VAR.getResult();  \
    }                                          \
    ANGLE_EMPTY_STATEMENT

// TODO(jmadill): Introduce way to store errors to a const Context.
#define ANGLE_SWALLOW_ERR(EXPR)                                       \
    {                                                                 \
        auto ANGLE_LOCAL_VAR = EXPR;                                  \
        if (ANGLE_LOCAL_VAR.isError())                                \
        {                                                             \
            ERR() << "Unhandled internal error: " << ANGLE_LOCAL_VAR; \
        }                                                             \
    }                                                                 \
    ANGLE_EMPTY_STATEMENT

#undef ANGLE_LOCAL_VAR
#undef ANGLE_CONCAT2
#undef ANGLE_CONCAT1

#include "Error.inl"

#endif // LIBANGLE_ERROR_H_
