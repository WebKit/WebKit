#ifndef _TCUSTRINGTEMPLATE_HPP
#define _TCUSTRINGTEMPLATE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
 * \brief String template class.
 *//*--------------------------------------------------------------------*/

#include <deStringUtil.hpp>

#include <map>
#include <string>

namespace tcu
{

class StringTemplate
{
public:
    StringTemplate(void);
    StringTemplate(const std::string &str);
    StringTemplate(StringTemplate &&other);
    ~StringTemplate(void);

    void setString(const std::string &str);

    std::string specialize(const std::map<std::string, std::string> &params) const;

    template <typename... args_t>
    std::string format(args_t &&...args) const;

private:
    StringTemplate(const StringTemplate &);            // not allowed!
    StringTemplate &operator=(const StringTemplate &); // not allowed!

    std::string m_template;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * Utility to unpack consecutive arguments into a parameter map
 *//*--------------------------------------------------------------------*/
namespace detail
{
static constexpr const char *TOKENS[] = {"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10", "11", "12",
                                         "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25",
                                         "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38",
                                         "39", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "50", "51",
                                         "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63"};

template <size_t ARG_NUM, typename unpacked_t>
inline void unpackArgs(unpacked_t &)
{
}

template <size_t ARG_NUM, typename unpacked_t, typename arg_t, typename... args_t>
inline void unpackArgs(unpacked_t &unpacked, arg_t &&cur, args_t &&...args)
{
    static_assert(ARG_NUM < DE_LENGTH_OF_ARRAY(TOKENS), "ARG_NUM must be less than DE_LENGTH_OF_ARRAY(TOKENS)");
    unpacked[TOKENS[ARG_NUM]] = de::toString(cur);
    unpackArgs<ARG_NUM + 1>(unpacked, ::std::forward<args_t>(args)...);
}
} // namespace detail

/*--------------------------------------------------------------------*//*!
 * \brief Implementation of specialize() using a variable argument list
 *//*--------------------------------------------------------------------*/
template <typename... args_t>
std::string StringTemplate::format(args_t &&...args) const
{
    std::map<std::string, std::string> unpacked = {};
    detail::unpackArgs<0>(unpacked, ::std::forward<args_t>(args)...);
    return specialize(unpacked);
}

} // namespace tcu

#endif // _TCUSTRINGTEMPLATE_HPP
