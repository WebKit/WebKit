//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
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

namespace egl
{
class Error;
}  // namespace egl

namespace egl
{

class [[nodiscard]] Error final
{
  public:
    explicit inline Error(EGLint errorCode);
    Error(EGLint errorCode, std::string &&message);
    Error(EGLint errorCode, EGLint id, std::string &&message);
    inline Error(const Error &other);
    inline Error(Error &&other);
    inline ~Error() = default;

    inline Error &operator=(const Error &other);
    inline Error &operator=(Error &&other);

    inline EGLint getCode() const;
    inline EGLint getID() const;
    inline bool isError() const;

    inline void setCode(EGLint code);

    const std::string &getMessage() const;

    static inline Error NoError();

  private:
    void createMessageString() const;

    friend std::ostream &operator<<(std::ostream &os, const Error &err);

    EGLint mCode;
    EGLint mID;
    mutable std::unique_ptr<std::string> mMessage;
};

inline Error NoError()
{
    return Error::NoError();
}

}  // namespace egl

#define ANGLE_CONCAT1(x, y) x##y
#define ANGLE_CONCAT2(x, y) ANGLE_CONCAT1(x, y)
#define ANGLE_LOCAL_VAR ANGLE_CONCAT2(_localVar, __LINE__)

#define ANGLE_TRY_TEMPLATE(EXPR, FINALLY, FUNC)       \
    do                                                \
    {                                                 \
        auto ANGLE_LOCAL_VAR = EXPR;                  \
        FINALLY;                                      \
        if (ANGLE_UNLIKELY(IsError(ANGLE_LOCAL_VAR))) \
        {                                             \
            FUNC(ANGLE_LOCAL_VAR);                    \
        }                                             \
    } while (0)

#define ANGLE_RETURN(X) return X;
#define ANGLE_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, static_cast<void>(0), ANGLE_RETURN)
#define ANGLE_TRY_WITH_FINALLY(EXPR, FINALLY) ANGLE_TRY_TEMPLATE(EXPR, FINALLY, ANGLE_RETURN)

// TODO(jmadill): Remove after EGL error refactor. http://anglebug.com/42261727
#define ANGLE_SWALLOW_ERR(EXPR)                                       \
    do                                                                \
    {                                                                 \
        auto ANGLE_LOCAL_VAR = EXPR;                                  \
        if (ANGLE_UNLIKELY(IsError(ANGLE_LOCAL_VAR)))                 \
        {                                                             \
            ERR() << "Unhandled internal error: " << ANGLE_LOCAL_VAR; \
        }                                                             \
    } while (0)

#undef ANGLE_LOCAL_VAR
#undef ANGLE_CONCAT2
#undef ANGLE_CONCAT1

#define ANGLE_CHECK(CONTEXT, EXPR, MESSAGE, ERROR)                                    \
    do                                                                                \
    {                                                                                 \
        if (ANGLE_UNLIKELY(!(EXPR)))                                                  \
        {                                                                             \
            CONTEXT->handleError(ERROR, MESSAGE, __FILE__, ANGLE_FUNCTION, __LINE__); \
            return angle::Result::Stop;                                               \
        }                                                                             \
    } while (0)

namespace angle
{
// Result implements an explicit exception handling mechanism.  A value of Stop signifies an
// exception akin to |throw|.
// TODO: make incorrect usage of Stop consistent with the above expectation.
// http://anglebug.com/42266839
enum class [[nodiscard]] Result
{
    Continue,
    Stop,
};

// TODO(jmadill): Remove this when refactor is complete. http://anglebug.com/42261727
egl::Error ResultToEGL(Result result);
}  // namespace angle

// TODO(jmadill): Remove this when refactor is complete. http://anglebug.com/42261727
inline bool IsError(angle::Result result)
{
    return result == angle::Result::Stop;
}

// TODO(jmadill): Remove this when refactor is complete. http://anglebug.com/42261727
inline bool IsError(const egl::Error &err)
{
    return err.isError();
}

// TODO(jmadill): Remove this when refactor is complete. http://anglebug.com/42261727
inline bool IsError(bool value)
{
    return !value;
}

// Utility macro for handling implementation methods inside Validation.
#define ANGLE_HANDLE_VALIDATION_ERR(X) \
    do                                 \
    {                                  \
        (void)(X);                     \
        return false;                  \
    } while (0)

#define ANGLE_VALIDATION_TRY(EXPR) \
    ANGLE_TRY_TEMPLATE(EXPR, static_cast<void>(0), ANGLE_HANDLE_VALIDATION_ERR)

#include "Error.inc"

#endif  // LIBANGLE_ERROR_H_
