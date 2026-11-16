#ifndef _TCUDEFS_HPP
#define _TCUDEFS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Basic definitions.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "qpTestLog.h"

#include <string>
#include <stdexcept>

/*--------------------------------------------------------------------*//*!
 * \brief dEQP Common Test Framework
 *
 * Code in this namespace doesn't require support for any specific client
 * APIs.
 *//*--------------------------------------------------------------------*/
namespace tcu
{

//! Kill program. Called when a fatal error occurs.
void die(const char *format, ...) DE_PRINTF_FUNC_ATTR(1, 2);

//! Print to debug console.
void print(const char *format, ...) DE_PRINTF_FUNC_ATTR(1, 2);

//! Print nonfatal error.
void printError(const char *format, ...) DE_PRINTF_FUNC_ATTR(1, 2);

//! Base exception class for dEQP test framework.
class Exception : public std::runtime_error
{
public:
    Exception(const char *message, const char *expr, const char *file, int line);
    Exception(const std::string &message);
    virtual ~Exception(void) throw()
    {
    }

    const char *getMessage(void) const
    {
        return m_message.what();
    }

private:
    // std::runtime_error is used here as an immutable ref-counted string.
    // This allows the copy constructor in the class to be noexcept.
    const std::runtime_error m_message;
};

//! Base exception class for test exceptions that affect test result
class TestException : public Exception
{
public:
    TestException(const char *message, const char *expr, const char *file, int line, qpTestResult result);
    TestException(const std::string &message, qpTestResult result);
    virtual ~TestException(void) throw()
    {
    }

    qpTestResult getTestResult(void) const
    {
        return m_result;
    }
    virtual bool isFatal(void) const
    {
        return false;
    }

private:
    const qpTestResult m_result;
};

//! Exception for test errors.
class TestError : public TestException
{
public:
    TestError(const char *message, const char *expr, const char *file, int line, qpTestResult result);
    TestError(const char *message, const char *expr, const char *file, int line);
    TestError(const std::string &message, const char *expr, const char *file, int line);
    TestError(const std::string &message);
    virtual ~TestError(void) throw()
    {
    }
};

//! Exception for internal errors.
class InternalError : public TestException
{
public:
    InternalError(const char *message, const char *expr, const char *file, int line);
    InternalError(const std::string &message, const char *expr, const char *file, int line);
    InternalError(const std::string &message);
    virtual ~InternalError(void) throw()
    {
    }
};

//! Resource error. Tester will terminate if thrown out of test case.
class ResourceError : public TestException
{
public:
    ResourceError(const char *message, const char *expr, const char *file, int line);
    ResourceError(const std::string &message);
    virtual ~ResourceError(void) throw()
    {
    }

    virtual bool isFatal(void) const
    {
        return true;
    }
};

//! Not supported error.
class NotSupportedError : public TestException
{
public:
    NotSupportedError(const char *message, const char *expr, const char *file, int line);
    NotSupportedError(const std::string &message, const char *expr, const char *file, int line);
    NotSupportedError(const std::string &message);
    virtual ~NotSupportedError(void) throw()
    {
    }
};

//! Quality warning.
class QualityWarning : public TestException
{
public:
    QualityWarning(const char *message, const char *expr, const char *file, int line);
    QualityWarning(const std::string &message, const char *expr, const char *file, int line);
    QualityWarning(const std::string &message);
    virtual ~QualityWarning(void) throw()
    {
    }
};

//! Capability warning.
class CapabilityWarning : public TestException
{
public:
    CapabilityWarning(const char *message, const char *expr, const char *file, int line);
    CapabilityWarning(const std::string &message, const char *expr, const char *file, int line);
    CapabilityWarning(const std::string &message);
    virtual ~CapabilityWarning(void) throw()
    {
    }
};

//! EnforceDefaultContext
class EnforceDefaultContext : public TestException
{
public:
    EnforceDefaultContext(const char *message, const char *expr, const char *file, int line);
    EnforceDefaultContext(const std::string &message, const char *expr, const char *file, int line);
    EnforceDefaultContext(const std::string &message);
    virtual ~EnforceDefaultContext(void) throw()
    {
    }
};

//! EnforceDefaultInstance
class EnforceDefaultInstance : public TestException
{
public:
    EnforceDefaultInstance(const char *message, const char *expr, const char *file, int line);
    EnforceDefaultInstance(const std::string &message, const char *expr, const char *file, int line);
    EnforceDefaultInstance(const std::string &message);
    virtual ~EnforceDefaultInstance(void) throw()
    {
    }
};

} // namespace tcu

#define TCU_THROW_EXPR(ERRCLASS, MSG, EXPR) throw tcu::ERRCLASS(MSG, EXPR, __FILE__, __LINE__)

#define TCU_THROW(ERRCLASS, MSG) TCU_THROW_EXPR(ERRCLASS, MSG, nullptr)

#define TCU_CHECK_AND_THROW(ERRCLASS, X, MSG)  \
    do                                         \
    {                                          \
        if (!(!false && (X)))                  \
            TCU_THROW_EXPR(ERRCLASS, MSG, #X); \
    } while (false)

//! Throw TestError.
#define TCU_FAIL(MSG) TCU_THROW(TestError, MSG)

//! Throw TestError if condition X is not satisfied.
#define TCU_CHECK(X)                                               \
    do                                                             \
    {                                                              \
        if (!(!false && (X)))                                      \
            throw tcu::TestError(nullptr, #X, __FILE__, __LINE__); \
    } while (false)

//! Throw TestError if condition X is not satisfied.
#define TCU_CHECK_MSG(X, MSG)                                    \
    do                                                           \
    {                                                            \
        if (!(!false && (X)))                                    \
            throw tcu::TestError((MSG), #X, __FILE__, __LINE__); \
    } while (false)

//! Throw InternalError if condition X is not satisfied
#define TCU_CHECK_INTERNAL(X)                                          \
    do                                                                 \
    {                                                                  \
        if (!(!false && (X)))                                          \
            throw tcu::InternalError(nullptr, #X, __FILE__, __LINE__); \
    } while (false)

#endif // _TCUDEFS_HPP
