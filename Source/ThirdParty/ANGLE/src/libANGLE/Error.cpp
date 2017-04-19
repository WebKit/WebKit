//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Error.cpp: Implements the egl::Error and gl::Error classes which encapsulate API errors
// and optional error messages.

#include "libANGLE/Error.h"

#include "common/angleutils.h"
#include "common/debug.h"

#include <cstdarg>

namespace gl
{

Error::Error(GLenum errorCode, std::string &&message)
    : mCode(errorCode), mID(errorCode), mMessage(new std::string(std::move(message)))
{
}

Error::Error(GLenum errorCode, const char *msg, ...) : mCode(errorCode), mID(errorCode)
{
    va_list vararg;
    va_start(vararg, msg);
    createMessageString();
    *mMessage = FormatString(msg, vararg);
    va_end(vararg);
}

Error::Error(GLenum errorCode, GLuint id, const char *msg, ...) : mCode(errorCode), mID(id)
{
    va_list vararg;
    va_start(vararg, msg);
    createMessageString();
    *mMessage = FormatString(msg, vararg);
    va_end(vararg);
}

void Error::createMessageString() const
{
    if (!mMessage)
    {
        mMessage.reset(new std::string);
    }
}

const std::string &Error::getMessage() const
{
    createMessageString();
    return *mMessage;
}

bool Error::operator==(const Error &other) const
{
    if (mCode != other.mCode)
        return false;

    // TODO(jmadill): Compare extended error codes instead of strings.
    if ((!mMessage || !other.mMessage) && (!mMessage != !other.mMessage))
        return false;

    return (*mMessage == *other.mMessage);
}

bool Error::operator!=(const Error &other) const
{
    return !(*this == other);
}

std::ostream &operator<<(std::ostream &os, const Error &err)
{
    return gl::FmtHexShort(os, err.getCode());
}

namespace priv
{
template <GLenum EnumT>
ErrorStream<EnumT>::ErrorStream()
{
}

template <GLenum EnumT>
ErrorStream<EnumT>::operator gl::Error()
{
    return Error(EnumT, mErrorStream.str().c_str());
}

template class ErrorStream<GL_OUT_OF_MEMORY>;
template class ErrorStream<GL_INVALID_OPERATION>;

}  // namespace priv

}  // namespace gl

namespace egl
{

Error::Error(EGLint errorCode, const char *msg, ...) : mCode(errorCode), mID(0)
{
    va_list vararg;
    va_start(vararg, msg);
    createMessageString();
    *mMessage = FormatString(msg, vararg);
    va_end(vararg);
}

Error::Error(EGLint errorCode, EGLint id, const char *msg, ...) : mCode(errorCode), mID(id)
{
    va_list vararg;
    va_start(vararg, msg);
    createMessageString();
    *mMessage = FormatString(msg, vararg);
    va_end(vararg);
}

Error::Error(EGLint errorCode, EGLint id, const std::string &msg)
    : mCode(errorCode), mID(id), mMessage(new std::string(msg))
{
}

void Error::createMessageString() const
{
    if (!mMessage)
    {
        mMessage.reset(new std::string);
    }
}

const std::string &Error::getMessage() const
{
    createMessageString();
    return *mMessage;
}

std::ostream &operator<<(std::ostream &os, const Error &err)
{
    return gl::FmtHexShort(os, err.getCode());
}
}
