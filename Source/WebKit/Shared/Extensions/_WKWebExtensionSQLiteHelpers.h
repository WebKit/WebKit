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

#import "_WKWebExtensionSQLiteDatabase.h"
#import "_WKWebExtensionSQLiteDatatypeTraits.h"
#import "_WKWebExtensionSQLiteStatement.h"
#import <algorithm>
#import <functional>
#import <sqlite3.h>
#import <tuple>
#import <type_traits>
#import <utility>

#pragma GCC visibility push(hidden)

namespace WebKit {

#pragma mark - Interface

template <typename... Parameters>
void SQLiteStatementBindAllParameters(_WKWebExtensionSQLiteStatement *, Parameters&&...);

template <typename... Parameters>
_WKSQLiteErrorCode SQLiteDatabaseExecute(_WKWebExtensionSQLiteDatabase *, NSString *query, Parameters&&...);

template <typename... Parameters>
_WKWebExtensionSQLiteRowEnumerator *SQLiteDatabaseFetch(_WKWebExtensionSQLiteDatabase *, NSString *query, Parameters&&...);

bool SQLiteDatabaseEnumerate(_WKWebExtensionSQLiteDatabase *, NSError **, NSString *query, void (^enumerationBlock)(_WKWebExtensionSQLiteRow *row, BOOL *stop));

template <typename... Parameters>
bool SQLiteDatabaseEnumerateRows(_WKWebExtensionSQLiteDatabase *, NSError **, NSString *query, std::tuple<Parameters...>&&, void (^enumerationBlock)(_WKWebExtensionSQLiteRow *row, BOOL *stop));

template <typename... Parameters>
bool SQLiteDatabaseExecuteAndReturnError(_WKWebExtensionSQLiteDatabase *, NSError **, NSString *query, Parameters&&...);

inline bool SQLiteIsExecutionError(_WKSQLiteErrorCode errorCode)
{
    return errorCode != SQLITE_OK && errorCode != SQLITE_ROW && errorCode != SQLITE_DONE;
}

#pragma mark - Implementation

template <typename T>
struct IsTuple : std::false_type { };
template <typename... Args>
struct IsTuple<std::tuple<Args...>> : std::true_type { };

template <int currentParameterIndex, typename T, typename... Parameters>
void _SQLiteStatementBindAllParameters(_WKWebExtensionSQLiteStatement *, T&&, Parameters&&...);
template <int currentParameterIndex>
void _SQLiteStatementBindAllParameters(_WKWebExtensionSQLiteStatement *);
template <typename... Parameters>
_WKSQLiteErrorCode _SQLiteDatabaseExecuteAndReturnError(_WKWebExtensionSQLiteDatabase *, NSError **, NSString *query, Parameters&&...);

template <typename... Parameters>
void SQLiteStatementBindAllParameters(_WKWebExtensionSQLiteStatement *statement, Parameters&&... parameters)
{
    _SQLiteStatementBindAllParameters<1>(statement, std::forward<Parameters>(parameters)...);
}

template <typename... Parameters>
_WKSQLiteErrorCode SQLiteDatabaseExecute(_WKWebExtensionSQLiteDatabase *database, NSString *query, Parameters&&... parameters)
{
    return _SQLiteDatabaseExecuteAndReturnError(database, nullptr, query, std::forward<Parameters>(parameters)...);
}

template <typename... Parameters>
bool SQLiteDatabaseExecuteAndReturnError(_WKWebExtensionSQLiteDatabase *database, NSError **error, NSString *query, Parameters&&... parameters)
{
    _WKSQLiteErrorCode result = _SQLiteDatabaseExecuteAndReturnError(database, error, query, std::forward<Parameters>(parameters)...);
    return result == SQLITE_DONE || result == SQLITE_OK;
}

template <typename... Parameters>
_WKSQLiteErrorCode _SQLiteDatabaseExecuteAndReturnError(_WKWebExtensionSQLiteDatabase *database, NSError **error, NSString *query, Parameters&&... parameters)
{
    NSError *statementError;
    _WKWebExtensionSQLiteStatement *statement = [[_WKWebExtensionSQLiteStatement alloc] initWithDatabase:database query:query error:&statementError];
    if (!statement) {
        if (error)
            *error = statementError;
        return static_cast<_WKSQLiteErrorCode>(statementError.code);
    }

    SQLiteStatementBindAllParameters(statement, std::forward<Parameters>(parameters)...);

    int resultCode = [statement execute];
    [statement invalidate];

    if (SQLiteIsExecutionError(resultCode))
        [database reportErrorWithCode:resultCode statement:statement.handle error:error];

    return resultCode;
}

template <typename... Parameters>
_WKWebExtensionSQLiteRowEnumerator *SQLiteDatabaseFetch(_WKWebExtensionSQLiteDatabase *database, NSString *query, Parameters&&... parameters)
{
    _WKWebExtensionSQLiteStatement *statement = [[_WKWebExtensionSQLiteStatement alloc] initWithDatabase:database query:query];
    SQLiteStatementBindAllParameters(statement, std::forward<Parameters>(parameters)...);
    return [statement fetch];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, NSString *string)
{
    [statement bindString:string atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, int n)
{
    [statement bindInt:n atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, unsigned n)
{
    ASSERT(n <= INT_MAX);
    [statement bindInt:n atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, int64_t n)
{
    [statement bindInt64:n atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, uint64_t n)
{
    ASSERT(n <= INT64_MAX);
    [statement bindInt64:n atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, long n)
{
    _SQLiteStatementBindParameter(statement, index, static_cast<int64_t>(n));
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, unsigned long n)
{
    _SQLiteStatementBindParameter(statement, index, static_cast<uint64_t>(n));
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, double n)
{
    [statement bindDouble:n atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, BOOL n)
{
    [statement bindInt:!!n atParameterIndex:index];
}

inline void _SQLiteStatementBindParameter(_WKWebExtensionSQLiteStatement *statement, int index, NSData *data)
{
    [statement bindData:data atParameterIndex:index];
}

template <int currentParameterIndex, typename T, typename... Parameters>
void _SQLiteStatementBindAllParameters(_WKWebExtensionSQLiteStatement *statement, T&& parameter, Parameters&&... parameters)
{
    _SQLiteStatementBindParameter(statement, currentParameterIndex, std::forward<T>(parameter));
    _SQLiteStatementBindAllParameters<currentParameterIndex + 1>(statement, std::forward<Parameters>(parameters)...);
}

template <int currentParameterIndex>
void _SQLiteStatementBindAllParameters(_WKWebExtensionSQLiteStatement *)
{
}

template <typename Tuple, int index, int count>
typename std::enable_if<index < count, void>::type _SQLiteStatementBindTupleParameters(__unsafe_unretained _WKWebExtensionSQLiteStatement *statement, Tuple&& parameters)
{
    _SQLiteStatementBindParameter(statement, index + 1, std::get<index>(parameters));
    _SQLiteStatementBindTupleParameters<Tuple, index + 1, count>(statement, std::forward(parameters));
}

template <typename ReturnType, typename... Args>
class SQLiteEnumerationBlock {
public:
    typedef ReturnType (^BlockType)(Args...);
    typedef std::tuple<Args...> TupleType;

public:
    inline SQLiteEnumerationBlock(BlockType)
    {
    }

    template<int ...S>
    inline ReturnType _callBlockWithAllColumns(sqlite3_stmt* statement, BlockType enumerationBlock, std::integer_sequence<int, S...>)
    {
        return enumerationBlock(_WKWebExtensionSQLiteDatatypeTraits<typename std::tuple_element<S, TupleType>::type>::fetch(statement, S)...);
    }

    inline ReturnType callBlockWithAllColumns(sqlite3_stmt* statement, BlockType enumerationBlock)
    {
        return _callBlockWithAllColumns(statement, enumerationBlock, std::make_integer_sequence<int, std::tuple_size<TupleType>::value>());
    }
};

template<typename R, typename... Args>
typename std::enable_if<std::is_same<R, BOOL>::value, BOOL>::type _StatementCallBlockWithAllColumns(sqlite3_stmt* statement, R (^block)(Args...))
{
    return SQLiteEnumerationBlock<R, Args...>(block).callBlockWithAllColumns(statement, block);
}

#if defined(OBJC_BOOL_IS_BOOL) && !OBJC_BOOL_IS_BOOL
template<typename R, typename... Args>
typename std::enable_if<std::is_same<R, bool>::value, BOOL>::type _StatementCallBlockWithAllColumns(sqlite3_stmt* statement, R (^block)(Args...))
{
    return (BOOL)SQLiteEnumerationBlock<R, Args...>(block).callBlockWithAllColumns(statement, block);
}
#endif

template<typename R, typename... Args>
typename std::enable_if<std::is_void<R>::value, BOOL>::type _StatementCallBlockWithAllColumns(sqlite3_stmt* statement, R (^block)(Args...))
{
    SQLiteEnumerationBlock<R, Args...>(block).callBlockWithAllColumns(statement, block);
    return YES;
}

template<typename TupleType, int ...S>
BOOL _WBSStatementFetchColumnsInTuple(sqlite3_stmt* statement, TupleType&& tuple, std::integer_sequence<int, S...>)
{
    tuple = std::make_tuple(_WKWebExtensionSQLiteDatatypeTraits<typename std::remove_reference<typename std::tuple_element<S, TupleType>::type>::type>::fetch(statement, S)...);
    return YES;
}

template<typename TupleType>
BOOL WBSStatementFetchColumnsInTuple(sqlite3_stmt* statement, TupleType&& tuple)
{
    return _WBSStatementFetchColumnsInTuple(statement, std::forward<TupleType>(tuple), std::make_integer_sequence<int, std::tuple_size<TupleType>::value>());
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index >= Count && !IsTuple<typename std::tuple_element<Index, TupleType>::type>::value, BOOL>::type _SQLiteStatementBindOrStep(_WKWebExtensionSQLiteDatabase *database, sqlite3_stmt* statement, NSError **error, TupleType&& tuple)
{
    while (true) {
        int resultCode = sqlite3_step(statement);
        if (resultCode == SQLITE_DONE)
            return YES;

        if (resultCode != SQLITE_ROW) {
            [database reportErrorWithCode:resultCode statement:statement error:error];
            break;
        }

        if (!_StatementCallBlockWithAllColumns(statement, std::get<Index>(tuple)))
            break;
    }

    return NO;
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index >= Count && IsTuple<typename std::tuple_element<Index, TupleType>::type>::value, BOOL>::type _SQLiteStatementBindOrStep(_WKWebExtensionSQLiteDatabase *database, sqlite3_stmt* statement, NSError **error, TupleType&& tuple)
{
    int resultCode = sqlite3_step(statement);
    if (resultCode != SQLITE_ROW) {
        [database reportErrorWithCode:resultCode statement:statement error:error];
        return NO;
    }

    if (!WBSStatementFetchColumnsInTuple(statement, std::move(std::get<Index>(tuple))))
        return NO;

    resultCode = sqlite3_step(statement);
    if (resultCode != SQLITE_DONE) {
        [database reportErrorWithCode:resultCode statement:statement error:error];
        return NO;
    }

    return YES;
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index < Count, BOOL>::type _SQLiteStatementBindOrStep(_WKWebExtensionSQLiteDatabase *database, sqlite3_stmt* statement, NSError **error, TupleType&& tuple)
{
    if (int resultCode = _WKWebExtensionSQLiteDatatypeTraits<typename std::remove_const<typename std::remove_reference<typename std::tuple_element<Index, TupleType>::type>::type>::type>::bind(statement, Index + 1, std::get<Index>(tuple)) != SQLITE_OK) {
        [database reportErrorWithCode:resultCode statement:statement error:error];
        return NO;
    }

    return _SQLiteStatementBindOrStep<Index + 1, Count, TupleType>(database, statement, error, std::forward<TupleType>(tuple));
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index >= Count, BOOL>::type _SQLiteStatementBind(_WKWebExtensionSQLiteDatabase *database, sqlite3_stmt* statement, NSError **error, TupleType&& tuple)
{
    return YES;
}

template<int Index, int Count, typename TupleType>
typename std::enable_if<Index < Count, BOOL>::type _SQLiteStatementBind(_WKWebExtensionSQLiteDatabase *database, sqlite3_stmt* statement, NSError **error, TupleType&& tuple)
{
    if (int resultCode = _WKWebExtensionSQLiteDatatypeTraits<typename std::remove_const<typename std::remove_reference<typename std::tuple_element<Index, TupleType>::type>::type>::type>::bind(statement, Index + 1, std::get<Index>(tuple)) != SQLITE_OK) {
        [database reportErrorWithCode:resultCode statement:statement error:error];
        return NO;
    }

    return _SQLiteStatementBind<Index + 1, Count, TupleType>(database, statement, error, std::forward<TupleType>(tuple));
}

template<typename... Args>
BOOL SQLiteDatabaseEnumerate(_WKWebExtensionSQLiteDatabase *database, NSError **error, NSString *query, Args&&... args)
{
    _WKWebExtensionSQLiteStatement *statement = [[_WKWebExtensionSQLiteStatement alloc] initWithDatabase:database query:query error:error];
    if (!statement)
        return NO;

    BOOL result = _SQLiteStatementBindOrStep<0, sizeof...(args) - 1, typename std::tuple<Args...>>(database, statement.handle, error, std::forward_as_tuple(args...));
    [statement invalidate];
    return result;
}

template<typename... Args>
BOOL SQLiteDatabaseEnumerate(_WKWebExtensionSQLiteStatement *statement, NSError **error, Args&&... args)
{
    BOOL result = _SQLiteStatementBindOrStep<0, sizeof...(args) - 1, typename std::tuple<Args...>>(statement.database, statement.handle, error, std::forward_as_tuple(args...));

    [statement reset];
    return result;
}

template<typename... Parameters>
bool SQLiteDatabaseEnumerateRows(_WKWebExtensionSQLiteDatabase *database, NSError **error, NSString *query, std::tuple<Parameters...>&& parameters, void (^enumerationBlock)(_WKWebExtensionSQLiteRow *row, BOOL *stop))
{
    _WKWebExtensionSQLiteStatement *statement = [[_WKWebExtensionSQLiteStatement alloc] initWithDatabase:database query:query error:error];
    if (!statement)
        return false;

    _SQLiteStatementBind<0, std::tuple_size<std::tuple<Parameters...>>::value, typename std::tuple<Parameters...>>(database, statement.handle, error, std::move(parameters));

    BOOL result = [statement fetchWithEnumerationBlock:enumerationBlock error:error];
    [statement invalidate];
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

