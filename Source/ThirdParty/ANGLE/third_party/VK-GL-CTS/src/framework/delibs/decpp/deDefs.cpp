/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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

#include <stdexcept>
#include <sstream>

namespace de
{

void throwRuntimeError(const char *message, const char *expr, const char *file, int line)
{
    std::ostringstream msg;
    msg << (message ? message : "Runtime check failed") << ": ";
    if (expr)
        msg << '\'' << expr << '\'';
    msg << " at " << file << ":" << line;
    throw std::runtime_error(msg.str());
}

} // namespace de
