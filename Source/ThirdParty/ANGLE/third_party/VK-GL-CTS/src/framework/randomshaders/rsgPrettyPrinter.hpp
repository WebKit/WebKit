#ifndef _RSGPRETTYPRINTER_HPP
#define _RSGPRETTYPRINTER_HPP
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
 * \brief Shader Source Formatter.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgToken.hpp"

#include <string>
#include <sstream>

namespace rsg
{

class PrettyPrinter
{
public:
    PrettyPrinter(std::ostringstream &str);
    ~PrettyPrinter(void)
    {
    }

    void append(const TokenStream &tokens);

private:
    void processToken(const Token &token);

    static const char *getSimpleTokenStr(Token::Type token);

    std::string m_line;
    std::ostringstream &m_str;
    int m_indentDepth;
};

} // namespace rsg

#endif // _RSGPRETTYPRINTER_HPP
