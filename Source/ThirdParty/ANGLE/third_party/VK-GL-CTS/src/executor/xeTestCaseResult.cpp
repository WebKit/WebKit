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
 * \brief Test case result models.
 *//*--------------------------------------------------------------------*/

#include "xeTestCaseResult.hpp"

#include <iomanip>
#include <limits>

namespace xe
{

const char *getTestStatusCodeName(TestStatusCode code)
{
    switch (code)
    {
    case TESTSTATUSCODE_PASS:
        return "Pass";
    case TESTSTATUSCODE_FAIL:
        return "Fail";
    case TESTSTATUSCODE_QUALITY_WARNING:
        return "QualityWarning";
    case TESTSTATUSCODE_COMPATIBILITY_WARNING:
        return "CompatibilityWarning";
    case TESTSTATUSCODE_PENDING:
        return "Pending";
    case TESTSTATUSCODE_RUNNING:
        return "Running";
    case TESTSTATUSCODE_NOT_SUPPORTED:
        return "NotSupported";
    case TESTSTATUSCODE_RESOURCE_ERROR:
        return "ResourceError";
    case TESTSTATUSCODE_INTERNAL_ERROR:
        return "InternalError";
    case TESTSTATUSCODE_CANCELED:
        return "Canceled";
    case TESTSTATUSCODE_TIMEOUT:
        return "Timeout";
    case TESTSTATUSCODE_CRASH:
        return "Crash";
    case TESTSTATUSCODE_DISABLED:
        return "Disabled";
    case TESTSTATUSCODE_TERMINATED:
        return "Terminated";
    case TESTSTATUSCODE_WAIVER:
        return "Waived";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

namespace ri
{

List::List(void)
{
}

List::~List(void)
{
    for (std::vector<Item *>::iterator i = m_items.begin(); i != m_items.end(); i++)
        delete *i;
    m_items.clear();
}

std::ostream &operator<<(std::ostream &str, const NumericValue &value)
{
    switch (value.getType())
    {
    case NumericValue::NUMVALTYPE_FLOAT64:
        return str << std::setprecision(std::numeric_limits<double>::digits10 + 2) << value.getFloat64();

    case NumericValue::NUMVALTYPE_INT64:
        return str << value.getInt64();

    default:
        DE_ASSERT(value.getType() == NumericValue::NUMVALTYPE_EMPTY);
        return str;
    }
}

} // namespace ri
} // namespace xe
