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
#include "common/utilities.h"

#include <cstdarg>

namespace
{
std::unique_ptr<std::string> EmplaceErrorString(std::string &&message)
{
    return message.empty() ? std::unique_ptr<std::string>()
                           : std::unique_ptr<std::string>(new std::string(std::move(message)));
}
}  // anonymous namespace

namespace gl
{

Error::Error(GLenum errorCode, std::string &&message)
    : mCode(errorCode), mID(errorCode), mMessage(EmplaceErrorString(std::move(message)))
{
}

Error::Error(GLenum errorCode, GLuint id, std::string &&message)
    : mCode(errorCode), mID(id), mMessage(EmplaceErrorString(std::move(message)))
{
}

void Error::createMessageString() const
{
    if (!mMessage)
    {
        mMessage.reset(new std::string(GetGenericErrorMessage(mCode)));
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

}  // namespace gl

namespace egl
{

Error::Error(EGLint errorCode, std::string &&message)
    : mCode(errorCode), mID(errorCode), mMessage(EmplaceErrorString(std::move(message)))
{
}

Error::Error(EGLint errorCode, EGLint id, std::string &&message)
    : mCode(errorCode), mID(id), mMessage(EmplaceErrorString(std::move(message)))
{
}

void Error::createMessageString() const
{
    if (!mMessage)
    {
        mMessage.reset(new std::string(GetGenericErrorMessage(mCode)));
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

}  // namespace egl
