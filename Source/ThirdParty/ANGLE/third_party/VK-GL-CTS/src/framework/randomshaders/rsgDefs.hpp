#ifndef _RSGDEFS_HPP
#define _RSGDEFS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
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
 * \brief Random shader generator base definitions.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

#include <stdexcept>

/*--------------------------------------------------------------------*//*!
 * \brief Random shader generator
 *//*--------------------------------------------------------------------*/
namespace rsg
{

class Exception : public std::runtime_error
{
public:
    Exception(const std::string &message) : std::runtime_error(message)
    {
    }
};

} // namespace rsg

#endif // _RSGDEFS_HPP
