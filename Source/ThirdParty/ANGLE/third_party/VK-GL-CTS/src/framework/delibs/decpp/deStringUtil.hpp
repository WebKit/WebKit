#ifndef _DESTRINGUTIL_HPP
#define _DESTRINGUTIL_HPP
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
 * \brief String utilities.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"

#include <string>
#include <sstream>
#include <vector>

namespace de
{

void StringUtil_selfTest(void);

template <typename T>
inline std::string toString(const T &value)
{
    std::ostringstream s;
    s << value;
    return s.str();
}

std::string toLower(const std::string &s);
std::string toUpper(const std::string &s);
std::string capitalize(const std::string &s);
std::vector<std::string> splitString(const std::string &s, char delim = '\0');
std::string floatToString(float val, int precision);
bool beginsWith(const std::string &s, const std::string &prefix);
bool endsWith(const std::string &s, const std::string &suffix);
char toUpper(char c);
char toLower(char c);
bool isUpper(char c);
bool isLower(char c);
bool isDigit(char c);

} // namespace de

#endif // _DESTRINGUTIL_HPP
