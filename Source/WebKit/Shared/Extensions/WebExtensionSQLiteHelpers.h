/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebExtensionSQLiteDatabase.h"
#include "WebExtensionSQLiteDatatypeTraits.h"
#include "WebExtensionSQLiteStatement.h"

#pragma GCC visibility push(hidden)

namespace WebKit {

// MARK: Interface

template <typename... Parameters>
void SQLiteStatementBindAllParameters(Ref<WebExtensionSQLiteDatabase>, Parameters&&...);

template <typename... Parameters>
int SQLiteDatabaseExecute(Ref<WebExtensionSQLiteDatabase>, const String& query, Parameters&&...);

template <typename... Parameters>
Ref<WebExtensionSQLiteRowEnumerator> SQLiteDatabaseFetch(Ref<WebExtensionSQLiteDatabase>, const String& query, Parameters&&...);

bool SQLiteDatabaseEnumerate(Ref<WebExtensionSQLiteDatabase>, RefPtr<API::Error>&, const String& query, Function<void(RefPtr<WebExtensionSQLiteRow>, bool)>);

template <typename... Parameters>
bool SQLiteDatabaseEnumerateRows(Ref<WebExtensionSQLiteDatabase>, RefPtr<API::Error>&, const String& query, std::tuple<Parameters...>&&, Function<void(RefPtr<WebExtensionSQLiteRow>, bool)>);

template <typename... Parameters>
bool SQLiteDatabaseExecuteAndReturnError(Ref<WebExtensionSQLiteDatabase>, RefPtr<API::Error>&, const String& query, Parameters&&...);


inline bool SQLiteIsExecutionError(int errorCode)
{
    return errorCode != SQLITE_OK && errorCode != SQLITE_ROW && errorCode != SQLITE_DONE;
}

// MARK: Implementation

template <typename T>
struct IsTuple : std::false_type { };
template <typename... Args>
struct IsTuple<std::tuple<Args...>> : std::true_type { };

template <int currentParameterIndex, typename T, typename... Parameters>
void SQLiteStatementBindAllParameters(RefPtr<WebExtensionSQLiteStatement>, T&&, Parameters&&...);
template <int currentParameterIndex>
void SQLiteStatementBindAllParameters(RefPtr<WebExtensionSQLiteStatement>);
template <typename... Parameters>
int SQLiteDatabaseExecuteAndReturnIntError(Ref<WebExtensionSQLiteDatabase>, RefPtr<API::Error> = nullptr, const String& query = { }, Parameters&&...);

template <typename... Parameters>
void SQLiteStatementBindAllParameters(RefPtr<WebExtensionSQLiteStatement> statement, Parameters&&... parameters)
{
    SQLiteStatementBindAllParameters<1>(statement, std::forward<Parameters>(parameters)...);
}

template <typename... Parameters>
int SQLiteDatabaseExecute(Ref<WebExtensionSQLiteDatabase> database, const String& query, Parameters&&... parameters)
{
    return SQLiteDatabaseExecuteAndReturnIntError(database, nullptr, query, std::forward<Parameters>(parameters)...);
}

template <typename... Parameters>
bool SQLiteDatabaseExecuteAndReturnError(Ref<WebExtensionSQLiteDatabase> database, RefPtr<API::Error>& error, const String& query, Parameters&&... parameters)
{
    int result = SQLiteDatabaseExecuteAndReturnIntError(database, error, query, std::forward<Parameters>(parameters)...);
    return result == SQLITE_DONE || result == SQLITE_OK;
}

template <typename... Parameters>
int SQLiteDatabaseExecuteAndReturnIntError(Ref<WebExtensionSQLiteDatabase> database, RefPtr<API::Error> error, const String& query, Parameters&&... parameters)
{
    RefPtr<API::Error> statementError;
    RefPtr statement = WebExtensionSQLiteStatement::create(database, query, statementError);
    if (!statement) {
        if (error)
            error = statementError;
        return static_cast<int>(statementError->errorCode());
    }

    SQLiteStatementBindAllParameters(statement, std::forward<Parameters>(parameters)...);

    int resultCode = statement->execute();
    statement->invalidate();

    if (SQLiteIsExecutionError(resultCode))
        database->reportErrorWithCode(resultCode, statement->handle(), error);

    return resultCode;
}

template <typename... Parameters>
Ref<WebExtensionSQLiteRowEnumerator> SQLiteDatabaseFetch(Ref<WebExtensionSQLiteDatabase> database, const String& query, Parameters&&... parameters)
{
    RefPtr statement = WebExtensionSQLiteStatement::create(database, query, nullptr);
    SQLiteStatementBindAllParameters(statement, std::forward<Parameters>(parameters)...);
    return statement->fetch();
}

template <typename T>
inline void SQLiteStatementBindParameter(RefPtr<WebExtensionSQLiteStatement> statement, int index, const T& object)
{
    statement->bind(object, index);
}

template <>
inline void SQLiteStatementBindParameter<unsigned>(RefPtr<WebExtensionSQLiteStatement> statement, int index, const unsigned& object)
{
    ASSERT(object <= INT_MAX);
    statement->bind(static_cast<int>(object), index);
}

template <>
inline void SQLiteStatementBindParameter<uint64_t>(RefPtr<WebExtensionSQLiteStatement> statement, int index, const uint64_t& object)
{
    ASSERT(object <= INT64_MAX);
    statement->bind(static_cast<int64_t>(object), index);
}

template <>
inline void SQLiteStatementBindParameter<long>(RefPtr<WebExtensionSQLiteStatement> statement, int index, const long& object)
{
    SQLiteStatementBindParameter(statement, index, static_cast<int64_t>(object));
}

template <>
inline void SQLiteStatementBindParameter<bool>(RefPtr<WebExtensionSQLiteStatement> statement, int index, const bool& object)
{
    statement->bind(!!object, index);
}

template <int currentParameterIndex, typename T, typename... Parameters>
void SQLiteStatementBindAllParameters(RefPtr<WebExtensionSQLiteStatement> statement, T&& parameter, Parameters&&... parameters)
{
    SQLiteStatementBindParameter(statement, currentParameterIndex, std::forward<T>(parameter));
    SQLiteStatementBindAllParameters<currentParameterIndex + 1>(statement, std::forward<Parameters>(parameters)...);
}

template <int currentParameterIndex>
void SQLiteStatementBindAllParameters(RefPtr<WebExtensionSQLiteStatement>)
{
}

template <typename Tuple, int index, int count>
typename std::enable_if<index < count, void>::type SQLiteStatementBindTupleParameters(RefPtr<WebExtensionSQLiteStatement> statement, Tuple&& parameters)
{
    SQLiteStatementBindParameter(statement, index + 1, std::get<index>(parameters));
    SQLiteStatementBindTupleParameters<Tuple, index + 1, count>(statement, std::forward(parameters));
}

template <typename ReturnType, typename BlockType, typename... Args>
class SQLiteIteratorBlock {
public:
    typedef Function<ReturnType(Args...)> FunctionType;
    typedef std::tuple<Args...> TupleType;

public:
    inline SQLiteIteratorBlock(BlockType)
    {
    }

    template<int ...S>
    inline ReturnType callBlockWithAllColumns(sqlite3_stmt* statement, FunctionType enumerationBlock, std::integer_sequence<int, S...>)
    {
        return enumerationBlock(WebExtensionSQLiteDatatypeTraits<typename std::tuple_element<S, TupleType>::type>::fetch(statement, S)...);
    }

    inline ReturnType callBlockWithAllColumns(sqlite3_stmt* statement, FunctionType enumerationBlock)
    {
        return callBlockWithAllColumns(statement, enumerationBlock, std::make_integer_sequence<int, std::tuple_size<TupleType>::value>());
    }
};

template<typename R, typename... Args>
typename std::enable_if<std::is_same<R, bool>::value, bool>::type StatementCallBlockWithAllColumns(sqlite3_stmt* statement, Function<R(Args...)> block)
{
    return SQLiteIteratorBlock<R, Args...>(block).callBlockWithAllColumns(statement, block);
}

template<typename R, typename... Args>
typename std::enable_if<std::is_void<R>::value, bool>::type StatementCallBlockWithAllColumns(sqlite3_stmt* statement, Function<R(Args...)> block)
{
    SQLiteIteratorBlock<R, Args...>(block).callBlockWithAllColumns(statement, block);
    return true;
}

template<typename TupleType, int ...S>
bool WBSStatementFetchColumnsInTuple(sqlite3_stmt* statement, TupleType&& tuple, std::integer_sequence<int, S...>)
{
    tuple = std::make_tuple(WebExtensionSQLiteDatatypeTraits<typename std::remove_reference<typename std::tuple_element<S, TupleType>::type>::type>::fetch(statement, S)...);
    return true;
}

template<typename TupleType>
bool WBSStatementFetchColumnsInTuple(sqlite3_stmt* statement, TupleType&& tuple)
{
    return WBSStatementFetchColumnsInTuple(statement, std::forward<TupleType>(tuple), std::make_integer_sequence<int, std::tuple_size<TupleType>::value>());
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index >= Count && !IsTuple<typename std::tuple_element<Index, TupleType>::type>::value, bool>::type SQLiteStatementBindOrStep(Ref<WebExtensionSQLiteDatabase> database, sqlite3_stmt* statement, RefPtr<API::Error>& error, TupleType&& tuple)
{
    while (true) {
        int resultCode = sqlite3_step(statement);
        if (resultCode == SQLITE_DONE)
            return true;

        if (resultCode != SQLITE_ROW) {
            database->reportErrorWithCode(resultCode, statement, error);
            break;
        }

        if (!StatementCallBlockWithAllColumns(statement, std::get<Index>(tuple)))
            break;
    }

    return false;
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index >= Count && IsTuple<typename std::tuple_element<Index, TupleType>::type>::value, bool>::type SQLiteStatementBindOrStep(Ref<WebExtensionSQLiteDatabase> database, sqlite3_stmt* statement, RefPtr<API::Error>& error, TupleType&& tuple)
{
    int resultCode = sqlite3_step(statement);
    if (resultCode != SQLITE_ROW) {
        database->reportErrorWithCode(resultCode, statement, error);
        return false;
    }

    if (!WBSStatementFetchColumnsInTuple(statement, WTFMove(std::get<Index>(tuple))))
        return false;

    resultCode = sqlite3_step(statement);
    if (resultCode != SQLITE_DONE) {
        database->reportErrorWithCode(resultCode, statement, error);
        return false;
    }

    return true;
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index < Count, bool>::type SQLiteStatementBindOrStep(Ref<WebExtensionSQLiteDatabase> database, sqlite3_stmt* statement, RefPtr<API::Error>& error, TupleType&& tuple)
{
    if (int resultCode = WebExtensionSQLiteDatatypeTraits<typename std::remove_const<typename std::remove_reference<typename std::tuple_element<Index, TupleType>::type>::type>::type>::bind(statement, Index + 1, std::get<Index>(tuple)) != SQLITE_OK) {
        database->reportErrorWithCode(resultCode, statement, error);
        return false;
    }

    return SQLiteStatementBindOrStep<Index + 1, Count, TupleType>(database, statement, error, std::forward<TupleType>(tuple));
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index >= Count, bool>::type SQLiteStatementBind(Ref<WebExtensionSQLiteDatabase> database, sqlite3_stmt* statement, RefPtr<API::Error>& error, TupleType&& tuple)
{
    return true;
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index < Count, bool>::type SQLiteStatementBind(Ref<WebExtensionSQLiteDatabase> database, sqlite3_stmt* statement, RefPtr<API::Error>& error, TupleType&& tuple)
{
    if (int resultCode = WebExtensionSQLiteDatatypeTraits<typename std::remove_const<typename std::remove_reference<typename std::tuple_element<Index, TupleType>::type>::type>::type>::bind(statement, Index + 1, std::get<Index>(tuple)) != SQLITE_OK) {
        database->reportErrorWithCode(resultCode, statement, error);
        return false;
    }

    return SQLiteStatementBind<Index + 1, Count, TupleType>(database, statement, error, std::forward<TupleType>(tuple));
}

template<typename... Args>
bool SQLiteDatabaseEnumerate(Ref<WebExtensionSQLiteDatabase> database, RefPtr<API::Error>& error, const String& query, Args&&... args)
{
    RefPtr<API::Error> outError;
    RefPtr<WebExtensionSQLiteStatement> statement = WebExtensionSQLiteStatement::create(database, query, outError);
    error = outError;
    if (outError)
        return false;

    bool result = SQLiteStatementBindOrStep<0, sizeof...(args) - 1, typename std::tuple<Args...>>(database, statement->handle(), error, std::forward_as_tuple(args...));
    statement->invalidate();
    return result;
}

template<typename... Args>
bool SQLiteDatabaseEnumerate(RefPtr<WebExtensionSQLiteStatement> statement, RefPtr<API::Error>& error, Args&&... args)
{
    bool result = SQLiteStatementBindOrStep<0, sizeof...(args) - 1, typename std::tuple<Args...>>(statement->database(), statement->handle(), error, std::forward_as_tuple(args...));

    statement->reset();
    return result;
}

template<typename... Parameters>
bool SQLiteDatabaseEnumerateRows(Ref<WebExtensionSQLiteDatabase> database, RefPtr<API::Error>& error, const String& query, std::tuple<Parameters...>&& parameters, Function<void(RefPtr<WebExtensionSQLiteRow>, bool)> enumerationBlock)
{
    RefPtr<API::Error> outError;
    RefPtr<WebExtensionSQLiteStatement> statement = WebExtensionSQLiteStatement::create(database, query, outError);
    error = outError;
    if (outError)
        return false;

    SQLiteStatementBind<0, std::tuple_size<std::tuple<Parameters...>>::value, typename std::tuple<Parameters...>>(database, statement->handle(), error, WTFMove(parameters));

    bool result = statement->fetchWithEnumerationCallback(enumerationBlock, error);
    statement->invalidate();
    return result;
}

} // namespace WebKit

using WebKit::SQLiteDatabaseEnumerate;
using WebKit::SQLiteDatabaseExecute;
using WebKit::SQLiteDatabaseExecuteAndReturnError;
using WebKit::SQLiteDatabaseFetch;
using WebKit::SQLiteIsExecutionError;
using WebKit::SQLiteStatementBindAllParameters;

#pragma GCC visibility pop
