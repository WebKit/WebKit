#ifndef _DECOMMANDLINE_HPP
#define _DECOMMANDLINE_HPP
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

#include "deDefs.hpp"

#include <map>
#include <string>
#include <vector>
#include <ostream>
#include <typeinfo>
#include <stdexcept>

namespace de
{
namespace cmdline
{

//! Default parsing function
template <typename ValueType>
void parseType(const char *src, ValueType *dst);

template <typename T>
struct NamedValue
{
    const char *name;
    T value;
};

template <typename OptName>
struct Option
{
    typedef typename OptName::ValueType ValueType;
    typedef void (*ParseFunc)(const char *src, ValueType *dst);

    // \note All assumed to point to static memory.
    const char *shortName;
    const char *longName;
    const char *description;
    const char *defaultValue; //!< Default value (parsed from string), or null if should not be set

    // \note Either parse or namedValues must be null.
    ParseFunc parse;                             //!< Custom parsing function or null.
    const NamedValue<ValueType> *namedValues;    //!< Named values or null.
    const NamedValue<ValueType> *namedValuesEnd; //!< Named value list end.

    //! Construct generic option (string, int, boolean).
    Option(const char *shortName_, const char *longName_, const char *description_, const char *defaultValue_ = nullptr)
        : shortName(shortName_)
        , longName(longName_)
        , description(description_)
        , defaultValue(defaultValue_)
        , parse(parseType<ValueType>)
        , namedValues(nullptr)
        , namedValuesEnd(0)
    {
    }

    //! Option with custom parsing function.
    Option(const char *shortName_, const char *longName_, const char *description_, ParseFunc parse_,
           const char *defaultValue_ = nullptr)
        : shortName(shortName_)
        , longName(longName_)
        , description(description_)
        , defaultValue(defaultValue_)
        , parse(parse_)
        , namedValues(nullptr)
        , namedValuesEnd(nullptr)
    {
    }

    //! Option that uses named values.
    Option(const char *shortName_, const char *longName_, const char *description_,
           const NamedValue<ValueType> *namedValues_, const NamedValue<ValueType> *namedValuesEnd_,
           const char *defaultValue_ = nullptr)
        : shortName(shortName_)
        , longName(longName_)
        , description(description_)
        , defaultValue(defaultValue_)
        , parse(nullptr)
        , namedValues(namedValues_)
        , namedValuesEnd(namedValuesEnd_)
    {
    }

    //! Option that uses named values.
    template <size_t NumNamedValues>
    Option(const char *shortName_, const char *longName_, const char *description_,
           const NamedValue<ValueType> (&namedValues_)[NumNamedValues], const char *defaultValue_ = nullptr)
        : shortName(shortName_)
        , longName(longName_)
        , description(description_)
        , defaultValue(defaultValue_)
        , parse(nullptr)
        , namedValues(DE_ARRAY_BEGIN(namedValues_))
        , namedValuesEnd(DE_ARRAY_END(namedValues_))
    {
    }
};

template <class Option>
struct OptTraits
{
    typedef typename Option::ValueType ValueType;
};

//! Default value lookup
template <typename ValueType>
inline void getTypeDefault(ValueType *dst)
{
    *dst = ValueType();
}

template <>
void getTypeDefault<bool>(bool *dst);

template <typename T>
inline bool isBoolean(void)
{
    return false;
}
template <>
inline bool isBoolean<bool>(void)
{
    return true;
}

//! Is argument boolean-only value?
template <class Option>
inline bool isBooleanOpt(void)
{
    return isBoolean<typename OptTraits<Option>::ValueType>();
}

namespace detail
{

using std::map;
using std::string;
using std::vector;

// TypedFieldMap implementation

template <class Name>
struct TypedFieldTraits
{
    // Generic implementation for cmdline.
    typedef typename OptTraits<Name>::ValueType ValueType;
};

template <class Value>
struct TypedFieldValueTraits
{
    static void destroy(void *value)
    {
        delete (Value *)value;
    }
};

class TypedFieldMap
{
public:
    TypedFieldMap(void);
    ~TypedFieldMap(void);

    bool empty(void) const
    {
        return m_fields.empty();
    }
    void clear(void);

    template <typename Name>
    void set(typename TypedFieldTraits<Name>::ValueType *value);

    template <typename Name>
    void set(const typename TypedFieldTraits<Name>::ValueType &value);

    template <typename Name>
    bool contains(void) const;

    template <typename Name>
    const typename TypedFieldTraits<Name>::ValueType &get(void) const;

private:
    TypedFieldMap(const TypedFieldMap &);
    TypedFieldMap &operator=(const TypedFieldMap &);

    typedef void (*DestroyFunc)(void *);

    struct Entry
    {
        void *value;
        DestroyFunc destructor;

        Entry(void) : value(nullptr), destructor(0)
        {
        }
        Entry(void *value_, DestroyFunc destructor_) : value(value_), destructor(destructor_)
        {
        }
    };

    typedef std::map<const std::type_info *, Entry> Map;

    bool contains(const std::type_info *key) const;
    const Entry &get(const std::type_info *key) const;
    void set(const std::type_info *key, const Entry &value);

    Map m_fields;
};

template <typename Name>
inline void TypedFieldMap::set(typename TypedFieldTraits<Name>::ValueType *value)
{
    set(&typeid(Name), Entry(value, &TypedFieldValueTraits<typename TypedFieldTraits<Name>::ValueType>::destroy));
}

template <typename Name>
void TypedFieldMap::set(const typename TypedFieldTraits<Name>::ValueType &value)
{
    typename TypedFieldTraits<Name>::ValueType *copy = new typename TypedFieldTraits<Name>::ValueType(value);

    try
    {
        set<Name>(copy);
    }
    catch (...)
    {
        delete copy;
        throw;
    }
}

template <typename Name>
inline bool TypedFieldMap::contains(void) const
{
    return contains(&typeid(Name));
}

template <typename Name>
inline const typename TypedFieldTraits<Name>::ValueType &TypedFieldMap::get(void) const
{
    return *static_cast<typename TypedFieldTraits<Name>::ValueType *>(get(&typeid(Name)).value);
}

class CommandLine;

typedef void (*GenericParseFunc)(const char *src, void *dst);

class Parser
{
public:
    Parser(void);
    ~Parser(void);

    template <class OptType>
    void addOption(const Option<OptType> &option);

    bool parse(int numArgs, const char *const *args, CommandLine *dst, std::ostream &err) const;

    void help(std::ostream &dst) const;

private:
    Parser(const Parser &);
    Parser &operator=(const Parser &);

    struct OptInfo;

    typedef void (*DispatchParseFunc)(const OptInfo *info, const char *src, TypedFieldMap *dst);
    typedef void (*SetDefaultFunc)(TypedFieldMap *dst);

    struct OptInfo
    {
        const char *shortName;
        const char *longName;
        const char *description;
        const char *defaultValue;
        bool isFlag; //!< Set true for bool typed arguments that do not used named values.

        GenericParseFunc parse;

        const void *namedValues;
        const void *namedValuesEnd;
        size_t namedValueStride;

        DispatchParseFunc dispatchParse;
        SetDefaultFunc setDefault;

        OptInfo(void)
            : shortName(nullptr)
            , longName(nullptr)
            , description(nullptr)
            , defaultValue(nullptr)
            , isFlag(false)
            , parse(nullptr)
            , namedValues(nullptr)
            , namedValuesEnd(nullptr)
            , namedValueStride(0)
            , dispatchParse(nullptr)
            , setDefault(nullptr)
        {
        }
    };

    void addOption(const OptInfo &option);

    template <typename OptName>
    static void dispatchParse(const OptInfo *info, const char *src, TypedFieldMap *dst);

    vector<OptInfo> m_options;
};

template <class OptType>
inline Parser &operator<<(Parser &parser, const Option<OptType> &option)
{
    parser.addOption(option);
    return parser;
}

//! Find match by name. Throws exception if no match is found.
const void *findNamedValueMatch(const char *src, const void *namedValues, const void *namedValuesEnd, size_t stride);

template <typename OptType>
void Parser::dispatchParse(const OptInfo *info, const char *src, TypedFieldMap *dst)
{
    typename OptTraits<OptType>::ValueType *value = new typename OptTraits<OptType>::ValueType();
    try
    {
        DE_ASSERT((!!info->parse) != (!!info->namedValues));
        if (info->parse)
        {
            ((typename Option<OptType>::ParseFunc)(info->parse))(src, value);
        }
        else
        {
            const void *match =
                findNamedValueMatch(src, info->namedValues, info->namedValuesEnd, info->namedValueStride);
            *value = static_cast<const NamedValue<typename OptTraits<OptType>::ValueType> *>(match)->value;
        }
        dst->set<OptType>(value);
    }
    catch (...)
    {
        delete value;
        throw;
    }
}

template <typename OptType>
void dispatchSetDefault(TypedFieldMap *dst)
{
    typename OptTraits<OptType>::ValueType *value = new typename OptTraits<OptType>::ValueType();
    try
    {
        getTypeDefault<typename OptTraits<OptType>::ValueType>(value);
        dst->set<OptType>(value);
    }
    catch (...)
    {
        delete value;
        throw;
    }
}

template <typename OptType>
const char *getNamedValueName(const void *value)
{
    const NamedValue<typename OptTraits<OptType>::ValueType> *typedVal =
        static_cast<const NamedValue<typename OptTraits<OptType>::ValueType>>(value);
    return typedVal->name;
}

template <typename OptType>
void setFromNamedValue(const void *value, TypedFieldMap *dst)
{
    const NamedValue<typename OptTraits<OptType>::ValueType> *typedVal =
        static_cast<const NamedValue<typename OptTraits<OptType>::ValueType>>(value);
    dst->set<OptType>(typedVal->value);
}

template <class OptType>
void Parser::addOption(const Option<OptType> &option)
{
    OptInfo opt;

    opt.shortName        = option.shortName;
    opt.longName         = option.longName;
    opt.description      = option.description;
    opt.defaultValue     = option.defaultValue;
    opt.isFlag           = isBooleanOpt<OptType>() && !option.namedValues;
    opt.parse            = (GenericParseFunc)option.parse;
    opt.namedValues      = (const void *)option.namedValues;
    opt.namedValuesEnd   = (const void *)option.namedValuesEnd;
    opt.namedValueStride = sizeof(*option.namedValues);
    opt.dispatchParse    = dispatchParse<OptType>;

    if (opt.isFlag)
        opt.setDefault = dispatchSetDefault<OptType>;

    addOption(opt);
}

class CommandLine
{
public:
    CommandLine(void)
    {
    }
    ~CommandLine(void)
    {
    }

    void clear(void);

    const TypedFieldMap &getOptions(void) const
    {
        return m_options;
    }
    const vector<string> &getArgs(void) const
    {
        return m_args;
    }

    template <typename Option>
    bool hasOption(void) const
    {
        return m_options.contains<Option>();
    }

    template <typename Option>
    const typename TypedFieldTraits<Option>::ValueType &getOption(void) const
    {
        return m_options.get<Option>();
    }

    bool helpSpecified(void) const;

private:
    TypedFieldMap m_options;
    vector<string> m_args;

    friend class Parser;
};

} // namespace detail

using detail::CommandLine;
using detail::Parser;

void selfTest(void);

} // namespace cmdline
} // namespace de

#define DE_DECLARE_COMMAND_LINE_OPT(NAME, TYPE) \
    struct NAME                                 \
    {                                           \
        typedef TYPE ValueType;                 \
    }

#endif // _DECOMMANDLINE_HPP
