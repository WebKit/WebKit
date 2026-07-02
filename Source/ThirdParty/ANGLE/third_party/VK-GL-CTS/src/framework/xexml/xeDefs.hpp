#ifndef _XEDEFS_HPP
#define _XEDEFS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Executor base definitions.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

#include <string>
#include <stdexcept>

namespace xe
{

class Error : public std::runtime_error
{
public:
    Error(const std::string &message) : std::runtime_error(message)
    {
    }
    Error(const char *message, const char *expr, const char *file, int line);
};

class ParseError : public Error
{
public:
    ParseError(const std::string &message) : Error(message)
    {
    }
};

} // namespace xe

#define XE_FAIL(MSG) throw xe::Error(MSG, "", __FILE__, __LINE__)
#define XE_CHECK(X)                                        \
    do                                                     \
    {                                                      \
        if ((!false && (X)) ? false : true)                \
            throw xe::Error(NULL, #X, __FILE__, __LINE__); \
    } while (false)
#define XE_CHECK_MSG(X, MSG)                              \
    do                                                    \
    {                                                     \
        if ((!false && (X)) ? false : true)               \
            throw xe::Error(MSG, #X, __FILE__, __LINE__); \
    } while (false)

#endif // _XEDEFS_HPP
