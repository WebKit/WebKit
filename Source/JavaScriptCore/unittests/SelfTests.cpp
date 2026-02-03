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

/*
    These are self-tests/usage examples of the framework.
*/

#include "config.h"

#include "JSObjectInlines.h"
#include "JSString.h"
#include "Test.h" // <- Include this header into your own test module

namespace JSC {

// A test is defined using the macro DEFINE_TEST. It is automatically picked up by the
// framework to be executed together with all other tests. The macro takes three
// parameters: 1) the test name; 2) a string with JavaScript code; 3) a C++ block.
//
// 1) The name identifies the test in a test run log. It must be unique within the same
// C++ file. It is okay to have duplicates across different compilation units, however
// it might be confusing. The test runner can filter tests to only run those with a
// given prefix, so it is a good idea to give related tests a unique prefix, like
// 'SelfTest_' in this file.
//
// 2) The second parameter is a string with JavaScript code. The code is evaluated to kick
// off the test execution in a newly created JSGlobalObject with the following
// additional bindings:
//
//  - load - The 'load' function for loading .js files, just like in jsc shell.
//  - $vm - The standard JSDollarVM.
//  - $assert - An object with an assortment of functions mostly mimicking
//              'JSTests/wasm/assert.js' API.
//  - $probe - A function calling the probe code (see below).
//
// If the JavaScript code completes without any exceptions thrown, the test counts
// as a success. If a JavaScript exception is thrown, the test counts as a failure.
//
// 3) The third parameter is the probe. It is a block of C++ code executed when the
// '$probe()' function is called in JavaScript. The function can be called with any number
// of arguments. In the probe body, the following variables are implicitly available:
//
//  - Vector<JSValue>& arguments; // All arguments passed to the '$probe()' call.
//  - JSGlobalObject* globalObject;
//  - ThrowScope throwScope;
//
// With this arrangement, the JS code sets up the test, creating any necessary objects
// that are easier to create in JavaScript. The probe code then performs behind-the-scenes
// manipulations and checks.
//
DEFINE_TEST(SelfTests_example,
    R"(
        const obj = {
            name: "foo",
            value: 42
        };
        $probe(obj);
    )",
    {
        JSObject* obj = arguments[0].getObject();
        // The CHECK macro fails the test if the argument is false.
        CHECK(obj);

        // GET_PROPERTY is a shortcut for getting the value of a named property.
        // The value (a JSValue) is then converted to a WTF::String and assigned to a
        // local variable 'String name' by EXTRACT_STRING, with all the necessary
        // exception checks along the way.
        EXTRACT_STRING(name, GET_PROPERTY(obj, "name"_s));
        CHECK_EQ("foo"_s, name);

        // Similar for 'int32_t value', setting it to the value of obj.value
        EXTRACT_INT32(value, GET_PROPERTY(obj, "value"_s));
        CHECK_EQ(42, value);
    }
)

// This illustrates the ability to return a value from a probe. If a probe returns a
// value, the value is returned from the '$probe()' call on the JS side. A probe without
// an explicit return implicitly returns 'undefined'. This way a probe can pass values
// back to JS for further validation, if that validation is easier to do in JavaScript.
//
DEFINE_TEST(SelfTests_probeReturnsValue,
    R"(
        const x = $probe();
        $assert.eq(x, 42);
    )",
    {
        return jsNumber(42);
    }
)

// The EXPECT_EXCEPTION_AFTER macro changes the execution mode so that an exception is
// expected be thrown between the macro and the end of the containing block. If no
// exception is thrown, the test fails.
//
DEFINE_TEST(SelfTests_fail,
    "$probe()",
    {
        EXPECT_EXCEPTION_AFTER;
        FAIL("just testing"_s);
    }
)

DEFINE_TEST(SelfTests_getPropertyUndefinedIfMissing,
    "$probe({})",
    {
        JSObject* obj = arguments[0].getObject();
        CHECK(obj);
        JSValue value = GET_PROPERTY(obj, "bogus"_s);
        CHECK_UNDEFINED(value);
    }
)

DEFINE_TEST(SelfTests_check,
    "$probe(42)",
    {
        JSValue arg = arguments[0];
        CHECK(arg.isNumber());
        EXPECT_EXCEPTION_AFTER;
        CHECK(arg.isString());
    }
)

DEFINE_TEST(SelfTests_checkEq,
    "$probe()",
    {
        CHECK_EQ(1, 1);
        EXPECT_EXCEPTION_AFTER;
        CHECK_EQ(0, 1);
    }
)

// This illustrates the key assertions available under $assert.
//
DEFINE_TEST(SelfTests_jsAssert,
    R"(
        $assert.eq(0, 0);
        $assert.eq(42, 42);
        $assert.eq("foo", "foo");
        $assert.eq(true, true);
        $assert.eq(false, false);
        $assert.truthy(true);
        $assert.truthy(1);
        $assert.falsy(false);
        $assert.falsy(0);
    )",
    {
    }
)

} // namespace JSC
