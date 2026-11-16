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
 * \brief Command line parser.
 *//*--------------------------------------------------------------------*/

#include "deCommandLine.hpp"

#include <set>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace de
{
namespace cmdline
{

namespace
{
DE_DECLARE_COMMAND_LINE_OPT(Help, bool);
}

namespace detail
{

inline const char *getNamedValueName(const void *namedValue)
{
    return static_cast<const NamedValue<uint8_t> *>(namedValue)->name;
}

using std::set;

TypedFieldMap::TypedFieldMap(void)
{
}

TypedFieldMap::~TypedFieldMap(void)
{
    clear();
}

void TypedFieldMap::clear(void)
{
    for (Map::const_iterator iter = m_fields.begin(); iter != m_fields.end(); ++iter)
    {
        if (iter->second.value)
            iter->second.destructor(iter->second.value);
    }
    m_fields.clear();
}

bool TypedFieldMap::contains(const std::type_info *key) const
{
    return m_fields.find(key) != m_fields.end();
}

const TypedFieldMap::Entry &TypedFieldMap::get(const std::type_info *key) const
{
    Map::const_iterator pos = m_fields.find(key);
    if (pos != m_fields.end())
        return pos->second;
    else
        throw std::out_of_range("Value not set");
}

void TypedFieldMap::set(const std::type_info *key, const Entry &value)
{
    Map::iterator pos = m_fields.find(key);

    if (pos != m_fields.end())
    {
        pos->second.destructor(pos->second.value);
        pos->second.value = nullptr;

        pos->second = value;
    }
    else
        m_fields.insert(std::make_pair(key, value));
}

Parser::Parser(void)
{
    addOption(Option<Help>("h", "help", "Show this help"));
}

Parser::~Parser(void)
{
}

void Parser::addOption(const OptInfo &option)
{
    m_options.push_back(option);
}

bool Parser::parse(int numArgs, const char *const *args, CommandLine *dst, std::ostream &err) const
{
    typedef map<string, const OptInfo *> OptMap;
    typedef set<const OptInfo *> OptSet;

    OptMap shortOptMap;
    OptMap longOptMap;
    OptSet seenOpts;
    bool allOk = true;

    DE_ASSERT(dst->m_args.empty() && dst->m_options.empty());

    for (vector<OptInfo>::const_iterator optIter = m_options.begin(); optIter != m_options.end(); optIter++)
    {
        const OptInfo &opt = *optIter;

        DE_ASSERT(opt.shortName || opt.longName);

        if (opt.shortName)
        {
            DE_ASSERT(shortOptMap.find(opt.shortName) == shortOptMap.end());
            shortOptMap[opt.shortName] = &opt;
        }

        if (opt.longName)
        {
            DE_ASSERT(longOptMap.find(opt.longName) == longOptMap.end());
            longOptMap[opt.longName] = &opt;
        }

        // Set default values.
        if (opt.defaultValue)
            opt.dispatchParse(&opt, opt.defaultValue, &dst->m_options);
        else if (opt.setDefault)
            opt.setDefault(&dst->m_options);
    }

    DE_ASSERT(!dst->helpSpecified());

    for (int argNdx = 0; argNdx < numArgs; argNdx++)
    {
        const char *arg = args[argNdx];
        int argLen      = (int)strlen(arg);

        if (arg[0] == '-' && arg[1] == '-' && arg[2] == 0)
        {
            // End of option list (--)
            for (int optNdx = argNdx + 1; optNdx < numArgs; optNdx++)
                dst->m_args.push_back(args[optNdx]);
            break;
        }
        else if (arg[0] == '-')
        {
            const bool isLongName         = arg[1] == '-';
            const char *nameStart         = arg + (isLongName ? 2 : 1);
            const char *nameEnd           = std::find(nameStart, arg + argLen, '=');
            const bool hasImmValue        = nameEnd != (arg + argLen);
            const OptMap &optMap          = isLongName ? longOptMap : shortOptMap;
            OptMap::const_iterator optPos = optMap.find(string(nameStart, nameEnd));
            const OptInfo *opt            = optPos != optMap.end() ? optPos->second : nullptr;

            if (!opt)
            {
                err << "Unrecognized command line option '" << arg << "'\n";
                allOk = false;
                continue;
            }

            if (seenOpts.find(opt) != seenOpts.end())
            {
                err << "Command line option '--" << opt->longName << "' specified multiple times\n";
                allOk = false;
                continue;
            }

            seenOpts.insert(opt);

            if (opt->isFlag)
            {
                if (!hasImmValue)
                {
                    opt->dispatchParse(opt, nullptr, &dst->m_options);
                }
                else
                {
                    err << "No value expected for command line option '--" << opt->longName << "'\n";
                    allOk = false;
                }
            }
            else
            {
                const bool hasValue = hasImmValue || (argNdx + 1 < numArgs);

                if (hasValue)
                {
                    const char *value = hasValue ? (hasImmValue ? nameEnd + 1 : args[argNdx + 1]) : nullptr;

                    if (!hasImmValue)
                        argNdx += 1; // Skip value

                    try
                    {
                        opt->dispatchParse(opt, value, &dst->m_options);
                    }
                    catch (const std::exception &e)
                    {
                        err << "Got error parsing command line option '--" << opt->longName << "': " << e.what()
                            << "\n";
                        allOk = false;
                    }
                }
                else
                {
                    err << "Expected value for command line option '--" << opt->longName << "'\n";
                    allOk = false;
                }
            }
        }
        else
        {
            // Not an option
            dst->m_args.push_back(arg);
        }
    }

    // Help specified?
    if (dst->helpSpecified())
        allOk = false;

    return allOk;
}

void Parser::help(std::ostream &str) const
{
    for (vector<OptInfo>::const_iterator optIter = m_options.begin(); optIter != m_options.end(); ++optIter)
    {
        const OptInfo &opt = *optIter;

        str << "  ";
        if (opt.shortName)
            str << "-" << opt.shortName;

        if (opt.shortName && opt.longName)
            str << ", ";

        if (opt.longName)
            str << "--" << opt.longName;

        if (opt.namedValues)
        {
            str << "=[";

            for (const void *curValue = opt.namedValues; curValue != opt.namedValuesEnd;
                 curValue             = (const void *)((uintptr_t)curValue + opt.namedValueStride))
            {
                if (curValue != opt.namedValues)
                    str << "|";
                str << getNamedValueName(curValue);
            }

            str << "]";
        }
        else if (!opt.isFlag)
            str << "=<value>";

        str << "\n";

        if (opt.description)
            str << "    " << opt.description << "\n";

        if (opt.defaultValue)
            str << "    default: '" << opt.defaultValue << "'\n";

        str << "\n";
    }
}

void CommandLine::clear(void)
{
    m_options.clear();
    m_args.clear();
}

bool CommandLine::helpSpecified(void) const
{
    return m_options.get<Help>();
}

const void *findNamedValueMatch(const char *src, const void *namedValues, const void *namedValuesEnd, size_t stride)
{
    std::string srcStr(src);

    for (const void *curValue = namedValues; curValue != namedValuesEnd;
         curValue             = (const void *)((uintptr_t)curValue + stride))
    {
        if (srcStr == getNamedValueName(curValue))
            return curValue;
    }

    throw std::invalid_argument("unrecognized value '" + srcStr + "'");
}

} // namespace detail

// Default / parsing functions

template <>
void getTypeDefault(bool *dst)
{
    *dst = false;
}

template <>
void parseType<bool>(const char *, bool *dst)
{
    *dst = true;
}

template <>
void parseType<std::string>(const char *src, std::string *dst)
{
    *dst = src;
}

template <>
void parseType<int>(const char *src, int *dst)
{
    std::istringstream str(src);
    str >> *dst;
    if (str.bad() || !str.eof())
        throw std::invalid_argument("invalid integer literal");
}

// Tests

DE_DECLARE_COMMAND_LINE_OPT(TestStringOpt, std::string);
DE_DECLARE_COMMAND_LINE_OPT(TestStringDefOpt, std::string);
DE_DECLARE_COMMAND_LINE_OPT(TestIntOpt, int);
DE_DECLARE_COMMAND_LINE_OPT(TestBoolOpt, bool);
DE_DECLARE_COMMAND_LINE_OPT(TestNamedOpt, uint64_t);

void selfTest(void)
{
    // Parsing with no options.
    {
        Parser parser;

        {
            std::ostringstream err;
            CommandLine cmdLine;
            const bool parseOk = parser.parse(0, nullptr, &cmdLine, err);

            DE_TEST_ASSERT(parseOk && err.str().empty());
        }

        {
            const char *args[] = {"-h"};
            std::ostringstream err;
            CommandLine cmdLine;
            const bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(err.str().empty()); // No message about -h
        }

        {
            const char *args[] = {"--help"};
            std::ostringstream err;
            CommandLine cmdLine;
            const bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(err.str().empty()); // No message about -h
        }

        {
            const char *args[] = {"foo", "bar", "baz baz"};
            std::ostringstream err;
            CommandLine cmdLine;
            const bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(parseOk && err.str().empty());
            DE_TEST_ASSERT(cmdLine.getArgs().size() == DE_LENGTH_OF_ARRAY(args));

            for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(args); ndx++)
                DE_TEST_ASSERT(cmdLine.getArgs()[ndx] == args[ndx]);
        }
    }

    // Parsing with options.
    {
        Parser parser;

        static const NamedValue<uint64_t> s_namedValues[] = {{"zero", 0}, {"one", 1}, {"huge", ~0ull}};

        parser << Option<TestStringOpt>("s", "string", "String option")
               << Option<TestStringDefOpt>("x", "xyz", "String option w/ default value", "foo")
               << Option<TestIntOpt>("i", "int", "Int option") << Option<TestBoolOpt>("b", "bool", "Test boolean flag")
               << Option<TestNamedOpt>("n", "named", "Test named opt", DE_ARRAY_BEGIN(s_namedValues),
                                       DE_ARRAY_END(s_namedValues), "one");

        {
            std::ostringstream err;
            DE_TEST_ASSERT(err.str().empty());
            parser.help(err);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Default values
        {
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(0, nullptr, &cmdLine, err);

            DE_TEST_ASSERT(parseOk);
            DE_TEST_ASSERT(err.str().empty());

            DE_TEST_ASSERT(!cmdLine.hasOption<TestStringOpt>());
            DE_TEST_ASSERT(!cmdLine.hasOption<TestIntOpt>());
            DE_TEST_ASSERT(cmdLine.getOption<TestNamedOpt>() == 1);
            DE_TEST_ASSERT(cmdLine.getOption<TestBoolOpt>() == false);
            DE_TEST_ASSERT(cmdLine.getOption<TestStringDefOpt>() == "foo");
        }

        // Basic parsing
        {
            const char *args[] = {"-s", "test value", "-b", "-i=9", "--named=huge"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(parseOk);
            DE_TEST_ASSERT(err.str().empty());

            DE_TEST_ASSERT(cmdLine.getOption<TestStringOpt>() == "test value");
            DE_TEST_ASSERT(cmdLine.getOption<TestIntOpt>() == 9);
            DE_TEST_ASSERT(cmdLine.getOption<TestBoolOpt>());
            DE_TEST_ASSERT(cmdLine.getOption<TestNamedOpt>() == ~0ull);
            DE_TEST_ASSERT(cmdLine.getOption<TestStringDefOpt>() == "foo");
        }

        // End of argument list (--)
        {
            const char *args[] = {"--string=foo", "-b", "--", "--int=2", "-b"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(parseOk);
            DE_TEST_ASSERT(err.str().empty());

            DE_TEST_ASSERT(cmdLine.getOption<TestStringOpt>() == "foo");
            DE_TEST_ASSERT(cmdLine.getOption<TestBoolOpt>());
            DE_TEST_ASSERT(!cmdLine.hasOption<TestIntOpt>());

            DE_TEST_ASSERT(cmdLine.getArgs().size() == 2);
            DE_TEST_ASSERT(cmdLine.getArgs()[0] == "--int=2");
            DE_TEST_ASSERT(cmdLine.getArgs()[1] == "-b");
        }

        // Value --
        {
            const char *args[] = {"--string", "--", "-b", "foo"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(parseOk);
            DE_TEST_ASSERT(err.str().empty());

            DE_TEST_ASSERT(cmdLine.getOption<TestStringOpt>() == "--");
            DE_TEST_ASSERT(cmdLine.getOption<TestBoolOpt>());
            DE_TEST_ASSERT(!cmdLine.hasOption<TestIntOpt>());

            DE_TEST_ASSERT(cmdLine.getArgs().size() == 1);
            DE_TEST_ASSERT(cmdLine.getArgs()[0] == "foo");
        }

        // Invalid flag usage
        {
            const char *args[] = {"-b=true"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Invalid named option
        {
            const char *args[] = {"-n=two"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Unrecognized option (-x)
        {
            const char *args[] = {"-x"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Unrecognized option (--xxx)
        {
            const char *args[] = {"--xxx"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Invalid int value
        {
            const char *args[] = {"--int", "1x"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Arg specified multiple times
        {
            const char *args[] = {"-s=2", "-s=3"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Missing value
        {
            const char *args[] = {"--int"};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(!parseOk);
            DE_TEST_ASSERT(!err.str().empty());
        }

        // Empty value --arg=
        {
            const char *args[] = {"--string=", "-b", "-x", ""};
            CommandLine cmdLine;
            std::ostringstream err;
            bool parseOk = parser.parse(DE_LENGTH_OF_ARRAY(args), &args[0], &cmdLine, err);

            DE_TEST_ASSERT(parseOk);
            DE_TEST_ASSERT(err.str().empty());
            DE_TEST_ASSERT(cmdLine.getOption<TestStringOpt>() == "");
            DE_TEST_ASSERT(cmdLine.getOption<TestStringDefOpt>() == "");
            DE_TEST_ASSERT(cmdLine.getOption<TestBoolOpt>());
        }
    }
}

} // namespace cmdline
} // namespace de
