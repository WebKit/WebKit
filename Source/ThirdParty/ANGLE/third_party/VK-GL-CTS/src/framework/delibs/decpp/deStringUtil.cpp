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

#include "deStringUtil.hpp"
#include "deString.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <locale>
#include <iomanip>
#include <cctype>

using std::istream_iterator;
using std::istringstream;
using std::locale;
using std::string;
using std::vector;

namespace de
{
namespace
{

// Always use locale::classic to ensure consistent behavior in all environments.

struct ToLower
{
    const locale &loc;
    ToLower(void) : loc(locale::classic())
    {
    }
    char operator()(char c)
    {
        return std::tolower(c, loc);
    }
};

struct ToUpper
{
    const locale &loc;
    ToUpper(void) : loc(locale::classic())
    {
    }
    char operator()(char c)
    {
        return std::toupper(c, loc);
    }
};

} // namespace

//! Convert string to lowercase using the classic "C" locale
string toLower(const string &str)
{
    string ret;
    std::transform(str.begin(), str.end(), std::inserter(ret, ret.begin()), ToLower());
    return ret;
}

//! Convert string to uppercase using the classic "C" locale
string toUpper(const string &str)
{
    string ret;
    std::transform(str.begin(), str.end(), std::inserter(ret, ret.begin()), ToUpper());
    return ret;
}

//! Convert string's first character to uppercase using the classic "C" locale
string capitalize(const string &str)
{
    if (str.empty())
        return str;
    return ToUpper()(str[0]) + str.substr(1);
}

//! Split a string into tokens. If `delim` is `'\0'`, separate by spans of
//! whitespace. Otherwise use a single character `delim` as the separator.

vector<string> splitString(const string &s, char delim)
{
    istringstream tokenStream(s);

    if (delim == '\0')
        return vector<string>(istream_iterator<string>(tokenStream), istream_iterator<string>());
    else
    {
        vector<string> ret;
        string token;

        while (std::getline(tokenStream, token, delim))
            ret.push_back(token);

        return ret;
    }
}

//! Convert floating-point value to string with fixed number of fractional decimals.
std::string floatToString(float val, int precision)
{
    std::ostringstream s;
    s << std::fixed << std::setprecision(precision) << val;
    return s.str();
}

bool beginsWith(const std::string &s, const std::string &prefix)
{
    return deStringBeginsWith(s.c_str(), prefix.c_str()) == true;
}

bool endsWith(const std::string &s, const std::string &suffix)
{
    if (suffix.length() > s.length())
        return false;
    else
    {
        const std::string::size_type offset = s.length() - suffix.length();
        return s.find(suffix, offset) == offset;
    }
}

char toUpper(char c)
{
    return std::toupper(c, std::locale::classic());
}

char toLower(char c)
{
    return std::tolower(c, std::locale::classic());
}

bool isUpper(char c)
{
    return std::isupper(c, std::locale::classic());
}

bool isLower(char c)
{
    return std::islower(c, std::locale::classic());
}

bool isDigit(char c)
{
    return std::isdigit(c, std::locale::classic());
}

void StringUtil_selfTest(void)
{

    DE_TEST_ASSERT(toString(42) == "42");
    DE_TEST_ASSERT(toString("foo") == "foo");
    DE_TEST_ASSERT(toLower("FooBar") == "foobar");
    DE_TEST_ASSERT(toUpper("FooBar") == "FOOBAR");

    {
        vector<string> tokens(splitString(" foo bar\n\tbaz   "));
        DE_TEST_ASSERT(tokens.size() == 3);
        DE_TEST_ASSERT(tokens[0] == "foo");
        DE_TEST_ASSERT(tokens[1] == "bar");
        DE_TEST_ASSERT(tokens[2] == "baz");
    }

    DE_TEST_ASSERT(floatToString(4, 1) == "4.0");

    DE_TEST_ASSERT(beginsWith("foobar", "foobar"));
    DE_TEST_ASSERT(beginsWith("foobar", "foo"));
    DE_TEST_ASSERT(beginsWith("foobar", "f"));
    DE_TEST_ASSERT(beginsWith("foobar", ""));
    DE_TEST_ASSERT(beginsWith("", ""));
    DE_TEST_ASSERT(!beginsWith("foobar", "bar"));
    DE_TEST_ASSERT(!beginsWith("foobar", "foobarbaz"));
    DE_TEST_ASSERT(!beginsWith("", "foo"));

    DE_TEST_ASSERT(endsWith("foobar", "foobar"));
    DE_TEST_ASSERT(endsWith("foobar", "bar"));
    DE_TEST_ASSERT(endsWith("foobar", "r"));
    DE_TEST_ASSERT(endsWith("foobar", ""));
    DE_TEST_ASSERT(endsWith("", ""));
    DE_TEST_ASSERT(!endsWith("foobar", "foo"));
    DE_TEST_ASSERT(!endsWith("foobar", "bazfoobar"));
    DE_TEST_ASSERT(!endsWith("foobar", "foobarbaz"));
    DE_TEST_ASSERT(!endsWith("", "foo"));

    DE_TEST_ASSERT(toUpper('a') == 'A');
    DE_TEST_ASSERT(toUpper('A') == 'A');
    DE_TEST_ASSERT(toLower('a') == 'a');
    DE_TEST_ASSERT(toLower('A') == 'a');
    DE_TEST_ASSERT(isUpper('A'));
    DE_TEST_ASSERT(!isUpper('a'));
    DE_TEST_ASSERT(isLower('a'));
    DE_TEST_ASSERT(!isLower('A'));
    DE_TEST_ASSERT(isDigit('0'));
    DE_TEST_ASSERT(!isDigit('a'));
}

} // namespace de
