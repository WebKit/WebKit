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

#include "tcuDefs.hpp"
#include "deFilePath.hpp"
#include "qpDebugOut.h"

#include <sstream>
#include <stdarg.h>

namespace tcu
{

void die(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    qpDiev(format, args);
    va_end(args);
}

void print(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    qpPrintv(format, args);
    va_end(args);
}

void printError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    qpPrintErrorv(format, args);
    va_end(args);
}

static std::string formatError(const char *message, const char *expr, const char *file, int line)
{
    std::ostringstream msg;
    msg << (message ? message : "Runtime check failed");

    if (expr)
        msg << ": '" << expr << '\'';

    if (file)
        msg << " at " << de::FilePath(file).getBaseName() << ":" << line;

    return msg.str();
}

Exception::Exception(const char *message, const char *expr, const char *file, int line)
    : std::runtime_error(formatError(message, expr, file, line))
    , m_message(message ? message : "Runtime check failed")
{
}

Exception::Exception(const std::string &message) : std::runtime_error(message), m_message(message)
{
}

TestException::TestException(const char *message, const char *expr, const char *file, int line, qpTestResult result)
    : Exception(formatError(message, expr, file, line))
    , m_result(result)
{
}

TestException::TestException(const std::string &message, qpTestResult result) : Exception(message), m_result(result)
{
}

TestError::TestError(const char *message, const char *expr, const char *file, int line, qpTestResult result)
    : TestException(message, expr, file, line, result)
{
}

TestError::TestError(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_FAIL)
{
}
TestError::TestError(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_FAIL)
{
}

TestError::TestError(const std::string &message) : TestException(message, QP_TEST_RESULT_FAIL)
{
}

InternalError::InternalError(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_INTERNAL_ERROR)
{
}

InternalError::InternalError(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_INTERNAL_ERROR)
{
}

InternalError::InternalError(const std::string &message) : TestException(message, QP_TEST_RESULT_INTERNAL_ERROR)
{
}

ResourceError::ResourceError(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_RESOURCE_ERROR)
{
}

ResourceError::ResourceError(const std::string &message) : TestException(message, QP_TEST_RESULT_RESOURCE_ERROR)
{
}

NotSupportedError::NotSupportedError(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_NOT_SUPPORTED)
{
}

NotSupportedError::NotSupportedError(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_NOT_SUPPORTED)
{
}

NotSupportedError::NotSupportedError(const std::string &message) : TestException(message, QP_TEST_RESULT_NOT_SUPPORTED)
{
}

QualityWarning::QualityWarning(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_QUALITY_WARNING)
{
}

QualityWarning::QualityWarning(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_QUALITY_WARNING)
{
}

QualityWarning::QualityWarning(const std::string &message) : TestException(message, QP_TEST_RESULT_QUALITY_WARNING)
{
}

EnforceDefaultContext::EnforceDefaultContext(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_ENFORCE_DEFAULT_CONTEXT)
{
}

EnforceDefaultContext::EnforceDefaultContext(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_ENFORCE_DEFAULT_CONTEXT)
{
}

EnforceDefaultContext::EnforceDefaultContext(const std::string &message)
    : TestException(message, QP_TEST_RESULT_ENFORCE_DEFAULT_CONTEXT)
{
}

EnforceDefaultInstance::EnforceDefaultInstance(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_ENFORCE_DEFAULT_INSTANCE)
{
}

EnforceDefaultInstance::EnforceDefaultInstance(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_ENFORCE_DEFAULT_INSTANCE)
{
}

EnforceDefaultInstance::EnforceDefaultInstance(const std::string &message)
    : TestException(message, QP_TEST_RESULT_ENFORCE_DEFAULT_INSTANCE)
{
}

CapabilityWarning::CapabilityWarning(const char *message, const char *expr, const char *file, int line)
    : TestException(message, expr, file, line, QP_TEST_RESULT_CAPABILITY_WARNING)
{
}

CapabilityWarning::CapabilityWarning(const std::string &message, const char *expr, const char *file, int line)
    : TestException(message.c_str(), expr, file, line, QP_TEST_RESULT_CAPABILITY_WARNING)
{
}

CapabilityWarning::CapabilityWarning(const std::string &message)
    : TestException(message, QP_TEST_RESULT_CAPABILITY_WARNING)
{
}

} // namespace tcu
