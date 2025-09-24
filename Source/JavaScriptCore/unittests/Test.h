/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Error.h"
#include "JSCJSValue.h"
#include "JSGlobalObject.h"

#include <wtf/Vector.h>

namespace JSC {

typedef JSValue (*TestProbe)(JSGlobalObject* globalObject, Vector<JSValue>& arguments);

struct Test {
    const char* name;
    const char* kickOffCode;
    TestProbe probe;
    bool isIgnored;
};

class ExceptionExpectedScope {
public:
    ExceptionExpectedScope(JSGlobalObject*, ThrowScope&, bool& isExceptionExpected);
    ~ExceptionExpectedScope();

private:
    JSGlobalObject* m_globalObject;
    ThrowScope& m_throwScope;
    bool& m_isExceptionExpected;
};

#define TEST_LINKER_SECTION_NAME jsc_unittests
#define TEST_LINKER_SECTION_NAME_STR "jsc_unittests"

#if OS(DARWIN)
#define TEST_LINKER_SECTION_ATTR __attribute__((section("__DATA_CONST," TEST_LINKER_SECTION_NAME_STR), used, retain))
#elif OS(WINDOWS)
// Not supported but build should still work.
#define TEST_LINKER_SECTION_ATTR [[maybe_unused]]
#else
#define TEST_LINKER_SECTION_ATTR __attribute__((section(TEST_LINKER_SECTION_NAME_STR), used, retain))
#endif

#define DEFINE_TEST_IMPL(_name, _kickOffCode, _probeBody, _ignored) \
    static JSValue test_probe_ ## _name(JSGlobalObject* globalObject, Vector<JSValue>& arguments __attribute__((unused))) \
    { \
        auto throwScope __attribute__((unused)) = DECLARE_THROW_SCOPE(globalObject->vm()); \
        throwScope.release(); /* we only need the scope for throwing, not validation, so release right away */ \
        bool __isExceptionExpected __attribute__((unused)) = false; \
        _probeBody \
        return jsUndefined(); \
    } \
    TEST_LINKER_SECTION_ATTR static Test _name = { \
        .name = #_name, \
        .kickOffCode = _kickOffCode, \
        .probe = test_probe_ ## _name, \
        .isIgnored = _ignored \
    };

// Define a test. See SelfTests.cpp for details.
#define DEFINE_TEST(_name, _kickOffCode, _probeBody) DEFINE_TEST_IMPL(_name, _kickOffCode, _probeBody, false)

// Define a test that will not be executed. This is a way to temporarily disable a failing test.
// Ignored tests are counted and reported as such in a test run summary.
#define DEFINE_IGNORED_TEST(_name, _kickOffCode, _probeBody) DEFINE_TEST_IMPL(_name, _kickOffCode, _probeBody, true)

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/*
    The following macros are provided for writing probes.
    They assume 'globalObject' and 'throwScope' are lexically visible.
*/

// Abort probe execution if there is a thrown JS exception.
#define CHECK_NO_EXCEPTION \
    RETURN_IF_EXCEPTION(throwScope, { })

// Throw a JS exception with the specified message and abort probe execution.
#define FAIL(message) \
    do { \
        JSObject* __error = createError(globalObject, message); \
        throwException(globalObject, throwScope, __error); \
        return jsNull(); \
    } while (false)

// Fail the test if the condition is false.
#define CHECK(condition) \
    if (!(condition)) \
        FAIL("CHECK failed at " __FILE__ ":" TOSTRING(__LINE__) ""_s)

// Fail the test if the condition is false, with the specified message as the exception text.
#define CHECK_MSG(condition, message) \
    if (!(condition)) \
        FAIL(message)

// Fail the test if the value is undefined.
#define CHECK_DEFINED(value) \
    CHECK_MSG(!(value).isUndefined(), "CHECK_DEFINED failed at " __FILE__ ":" TOSTRING(__LINE__) ""_s)

// Fail the test if the value is defined.
#define CHECK_UNDEFINED(value) \
    CHECK_MSG((value).isUndefined(), "CHECK_UNDEFINED failed at " __FILE__ ":" TOSTRING(__LINE__) ""_s)

// Fail the test if the two values are not equal in the sense of lhs == rhs.
#define CHECK_EQ(lhs, rhs) \
    do { \
        auto __lhs = lhs; \
        auto __rhs = rhs; \
        if (__lhs != __rhs) { \
            String __message = makeString("Not equal: ("_s, __lhs, ") and ("_s, __rhs, ") at " __FILE__ ":" TOSTRING(__LINE__) ""_s); \
            FAIL(__message); \
        } \
    } while (false)

// Retrieve the value of the object property by the specified name (producing a JSValue).
// If the property is not present, return 'undefined'.
#define GET_PROPERTY(object, propertyName) \
    (object)->get(globalObject, PropertyName(Identifier::fromString(globalObject->vm(), propertyName)))

// Define a variable 'String var' and set it to the WTF::String with the contents
// of 'expr' (a JSValue), which must be a JSString. Fail if the value is not a JSString.
#define EXTRACT_STRING(var, expr) \
    String var; \
    do { \
        auto __value = expr; \
        CHECK_NO_EXCEPTION; \
        var = __value.toWTFString(globalObject); \
        CHECK_NO_EXCEPTION; \
    } while (false)

// Define a variable 'int32_t var' and set it to the result of converting 'expr' (a
// JSValue) to int32. Fail if the value can't be converted to int32.
#define EXTRACT_INT32(var, expr) \
    int32_t var; \
    do { \
        auto __value = expr; \
        CHECK_NO_EXCEPTION; \
        var = __value.toInt32(globalObject); \
        CHECK_NO_EXCEPTION; \
    } while (false)

#define EXCEPTION_EXPECTED_SCOPE \
    ExceptionExpectedScope(globalObject, throwScope, __isExceptionExpected)

// Change the execution mode so that a failure happening after this aborts the test but
// counts it as success, while if the current block completes without a failure, that in
// itself counts as a failure.
#define EXPECT_EXCEPTION_AFTER \
    auto scope = EXCEPTION_EXPECTED_SCOPE;

// Run all tests whose name begins with the given prefix.
// If the prefix is a nullptr, run all tests.
void runAllTests(const char* filterPrefix);

} // namespace JSC
