#ifndef _RSGPROGRAMGENERATOR_HPP
#define _RSGPROGRAMGENERATOR_HPP
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
 * \brief Program generator.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgParameters.hpp"
#include "rsgShader.hpp"

namespace rsg
{

class ProgramGenerator
{
public:
    ProgramGenerator(void);
    ~ProgramGenerator(void);

    void generate(const ProgramParameters &programParams, Shader &vertexShader, Shader &fragmentShader);

private:
    ProgramGenerator(const ProgramGenerator &other);
    ProgramGenerator &operator=(const ProgramGenerator &other);
};

} // namespace rsg

#endif // _RSGPROGRAMGENERATOR_HPP
