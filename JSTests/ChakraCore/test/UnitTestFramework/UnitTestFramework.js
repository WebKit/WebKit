//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Unit test framework. The one API that can be used by unit tests.
// Usage:
//   How to import UTF -- add the following to the beginning of your test:
//     /// <reference path="../UnitTestFramework/UnitTestFramework.js" />
//     if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
//         this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
//     }
//   How to define and run tests:
//     var tests = [ { name: "test name 1", body: function () {} }, ...];
//     testRunner.run(tests);
//   How to output "pass" only if run passes so that we can skip baseline:
//     testRunner.run(tests, {verbose: false});
//     Or do this only when "summary" is given on command line:
//          jshost   -args summary -endargs
//          testRunner.run(tests, { verbose: WScript.Arguments[0] != "summary" });
//   How to use assert:
//     assert.areEqual(expected, actual, "those two should be equal");
//     assert.areNotEqual(expected, actual, "those two should NOT be equal");
//     assert.fail("error");
//     assert.throws(function, SyntaxError);
//     assert.doesNotThrow(function, "this function should not throw anything");
//   Some useful helpers:
//     helpers.writeln("works in both", "console", "and", "browser);
//     helpers.printObject(WScript);
//     var isInBrowser = helpers.isInBrowser();

var helpers = function helpers() {
    //private
    var undefinedAsString = "undefined";
    var isWScriptAvailable = this.WScript;
    if (isWScriptAvailable && !this.WScript.LoadModuleFile) {
        WScript.LoadModuleFile = function (fileName) {
            WScript.LoadScriptFile(fileName, "module");
        }
    }

    return {
        isInBrowser: function isInBrowser() {
            return typeof (document) !== undefinedAsString;
        },

        // TODO: instead of this hack consider exposing this/ScriptConfiguration through WScript.
        get isCompatVersion9() {
            return (typeof (ArrayBuffer) == undefinedAsString);
        },
        get isVersion10OrLater() {
            return !this.isCompatVersion9;
        },

        // If propName is provided, thedummy object would have 1 property with name = propName, value = 0.
        getDummyObject: function getDummyObject(propName) {
            var dummy = this.isInBrowser() ? document : {};
            //var dummy = {};

            if (propName != undefined) {
                dummy[propName] = 0;
            }
            return dummy;
        },

        writeln: function writeln() {
            var line = "", i;
            for (i = 0; i < arguments.length; i += 1) {
                line = line.concat(arguments[i])
            }
            if (!this.isInBrowser() || isWScriptAvailable) {
                WScript.Echo(line);
            } else {
                document.writeln(line);
                document.writeln("<br/>");
            }
        },

        printObject: function printObject(o) {
            var name;
            for (name in o) {
                this.writeln(name, o.hasOwnProperty(name) ? "" : " (inherited)", ": ", o[name]);
            }
        },

        withPropertyDeleted: function withPropertyDeleted(object, propertyName, callback) {
            var descriptor = Object.getOwnPropertyDescriptor(object, propertyName);
            try {
                delete object[propertyName];
                callback();
            } finally {
                Object.defineProperty(object, propertyName, descriptor);
            }
        },
    }
}(); // helpers module.

var testRunner = function testRunner() {
    var executedTestCount = 0;
    var passedTestCount = 0;
    var objectType = "object";
    var _verbose = true; // If false, try to output "pass" only for passed run so we can skip baseline.

    return {
        runTests: function runTests(testsToRun, options) {
            ///<summary>Runs provided tests.<br/>
            /// The 'testsToRun' is an object that has enumerable properties,<br/>
            /// each property is an object that has 'name' and 'body' properties.
            ///</summary>

            if (options && options.hasOwnProperty("verbose")) {
                _verbose = options.verbose;
            }

            for (var i in testsToRun) {
                var isRunnable = typeof testsToRun[i] === objectType;
                if (isRunnable) {
                    this.runTest(i, testsToRun[i].name, testsToRun[i].body);
                }
            }

            if (_verbose || executedTestCount != passedTestCount) {
                helpers.writeln("Summary of tests: total executed: ", executedTestCount,
                  "; passed: ", passedTestCount, "; failed: ", executedTestCount - passedTestCount);
            } else {
                helpers.writeln("pass");
            }
        },

        run: function run(tests, options) {
            this.runTests(tests, options);
        },

        runTest: function runTest(testIndex, testName, testBody) {
            ///<summary>Runs test body catching all exceptions.<br/>
            /// Result: prints PASSED/FAILED to the output.
            ///</summary>
            function logTestNameIf(b) {
                if (b) {
                    helpers.writeln("*** Running test #", executedTestCount + 1, " (", testIndex, "): ", testName);
                }
            }

            logTestNameIf(_verbose);

            var isSuccess = true;
            try {
                testBody();
            } catch (ex) {
                var message = ex.stack || ex.message || ex;
                logTestNameIf(!_verbose);
                helpers.writeln("Test threw exception: ", message);
                isSuccess = false;
            }
            if (isSuccess) {
                if (_verbose) {
                    helpers.writeln("PASSED");
                }
                ++passedTestCount;
            } else {
                helpers.writeln("FAILED");
            }

            ++executedTestCount;
        }
    }
}(); // testRunner.

var assert = function assert() {
    // private
    var isObject = function isObject(x) {
        return x instanceof Object && typeof x !== "function";
    };

    var isNaN = function isNaN(x) {
        return x !== x;
    };

    // returns true on success, and error message on failure
    var compare = function compare(expected, actual) {
        if (expected === actual) {
            return true;
        } else if (isObject(expected)) {
            if (!isObject(actual)) return "actual is not an object";
            var expectedFieldCount = 0, actualFieldCount = 0;
            for (var i in expected) {
                var compareResult = compare(expected[i], actual[i]);
                if (compareResult !== true) return compareResult;
                ++expectedFieldCount;
            }
            for (var i in actual) {
                ++actualFieldCount;
            }
            if (expectedFieldCount !== actualFieldCount) {
                return "actual has different number of fields than expected";
            }
            return true;
        } else {
            if (isObject(actual)) return "actual is an object";
            if (isNaN(expected) && isNaN(actual)) return true;
            return "expected: " + expected + " actual: " + actual;
        }
    };

    var epsilon = (function () {
        var next, result;
        for (next = 1; 1 + next !== 1; next = next / 2) {
            result = next;
        }
        return result;
    }());

    var compareAlmostEqualRelative = function compareAlmostEqualRelative(expected, actual) {
        if (typeof expected !== "number" || typeof actual !== "number") {
            return compare(expected, actual);
        } else {
            var diff = Math.abs(expected - actual);
            var absExpected = Math.abs(expected);
            var absActual = Math.abs(actual);
            var largest = (absExpected > absActual) ? absExpected : absActual;

            var maxRelDiff = epsilon * 5;

            if (diff <= largest * maxRelDiff) {
                return true;
            } else {
                return "expected almost: " + expected + " actual: " + actual + " difference: " + diff;
            }
        }
    }

    var validate = function validate(result, assertType, message) {
        if (result !== true) {
            var exMessage = addMessage("assert." + assertType + " failed: " + result);
            exMessage = addMessage(exMessage, message);
            throw exMessage;
        }
    }

    var addMessage = function addMessage(baseMessage, message) {
        if (message !== undefined) {
            baseMessage += ": " + message;
        }
        return baseMessage;
    }

    return {
        areEqual: function areEqual(expected, actual, message) {
            /// <summary>
            /// IMPORTANT: NaN compares equal.<br/>
            /// IMPORTANT: for objects, assert.AreEqual checks the fields.<br/>
            /// So, for 'var obj1={}, obj2={}' areEqual would be success, although in Javascript obj1 != obj2.<br/><br/>
            /// Performs deep comparison of arguments.<br/>
            /// This works for objects and simple types.<br/><br/>
            ///
            /// TODO: account for other types?<br/>
            /// TODO: account for missing vs undefined fields.
            /// </summary>
            validate(compare(expected, actual), "areEqual", message);
        },

        areNotEqual: function areNotEqual(expected, actual, message) {
            validate(compare(expected, actual) !== true, "areNotEqual", message);
        },

        areAlmostEqual: function areAlmostEqual(expected, actual, message) {
            /// <summary>
            /// Compares numbers with an almost equal relative epsilon comparison.
            /// Useful for verifying math functions.
            /// </summary>
            validate(compareAlmostEqualRelative(expected, actual), "areAlmostEqual", message);
        },

        isTrue: function isTrue(actual, message) {
            validate(compare(true, actual), "isTrue", message);
        },

        isFalse: function isFalse(actual, message) {
            validate(compare(false, actual), "isFalse", message);
        },

        isUndefined: function isUndefined(actual, message) {
            validate(compare(undefined, actual), "isUndefined", message);
        },

        isNotUndefined: function isUndefined(actual, message) {
            validate(compare(undefined, actual) !== true, "isNotUndefined", message);
        },

        throws: function throws(testFunction, expectedException, message, expectedErrorMessage) {
            /// <summary>
            /// Makes sure that the function specified by the 'testFunction' parameter
            /// throws the exception specified by the 'expectedException' parameter.<br/><br/>
            ///
            /// expectedException:  A specific exception type, e.g. TypeError.
            ///                     if undefined, this will pass if testFunction throws any exception.<br/>
            /// message: Test-provided explanation of why this particular exception should be thrown.<br/>
            /// expectedErrorMessage: If specified, verifies the thrown Error has expected message.<br/>
            ///                       You only need to specify this if you are looking for a specific error message.<br/>
            ///                       Does not require the prefix of e.g. "TypeError:" that would be printed by the host.<br/><br/>
            ///
            /// Example:<br/>
            /// assert.throws(function() { eval("{"); }, SyntaxError, "expected SyntaxError")<br/>
            /// assert.throws(function() { eval("{"); }) // -- use this when you don't care which exception is thrown.<br/>
            /// assert.throws(function() { eval("{"); }, SyntaxError, "expected SyntaxError with message about expected semicolon.", "Expected ';'")
            /// </summary>
            var noException = {};         // Some unique object which will not be equal to anything else.
            var exception = noException;  // Set default value.
            try {
                testFunction();
            } catch (ex) {
                exception = ex;
            }

            if (expectedException === undefined && exception !== noException) {
                return;  // Any exception thrown is OK.
            }

            var validationPart = exception;
            if (exception !== noException && exception instanceof Object) {
                validationPart = exception.constructor;
            }
            var validationErrorMessage = exception instanceof Error ? exception.message : undefined;

            // Remap Chakra exception expectations to JSC exception expectations.
            var jscReplacements = [
                {
                    regexp: /Use before declaration/,
                    replStr: "Cannot access uninitialized variable."
                },
                {
                    regexp: /Assignment to const/,
                    replStr: "Attempted to assign to readonly property."
                },
                {
                    regexp: /'(\w+)' is undefined/,
                    replStr: "Can't find variable: $1"
                },
                {
                    regexp: /Object.prototype.__proto__: 'this' is not an Object/,
                    replStr: "Can't convert undefined or null to object"
                },
                {
                    regexp: /Object.setPrototypeOf: argument is not an Object and is not null/,
                    replStr: "Type error"
                },
                {
                    regexp: /Object.setPrototypeOf: argument is not an Object/,
                    replStr: "Type error"
                },
                {
                    regexp: /Cannot modify non-writable property '(\w+)'/,
                    replStr: "Attempted to assign to readonly property."
                },
                {
                    regexp: /'(\w+)' is not a constructor/,
                    replStr: "Reflect.construct requires the third argument be a constructor if present"
                },
                {
                    regexp: /The rest parameter must be the last parameter in a formals list\./,
                    replStr: "Unexpected token ','. Rest parameter should be the last parameter in a function declaration."
                },
                {
                    regexp: /Expected identifier/,
                    replStr: "Unexpected token ','. Expected a parameter pattern or a ')' in parameter list."
                },
                {
                    regexp: /Invalid unary operator on the left hand side of exponentiation \(\*\*\) operator/,
                    replStr: "Unexpected token '**'. Ambiguous unary expression in the left hand side of the exponentiation expression; parentheses must be used to disambiguate the expression."
                },
                {
                    regexp: /Assignment to read-only properties is not allowed in strict mode/,
                    replStr: "Attempted to assign to readonly property."
                },
                {
                    regexp: /(Array|Map|Set|String).prototype.(entries|find|findIndex|keys|repeat|values): 'this' is null or undefined/,
                    replStr: "$1.prototype.$2 requires that |this| not be null or undefined"
                },
                {
                    regexp: /(Array|Map|Set|String).prototype.entries: 'this' is not a \1 object/,
                    replStr: "Cannot create a $1 entry iterator for a non-$1 object."
                },
                {
                    regexp: /(Array|Map|Set|String).prototype.keys: 'this' is not a \1 object/,
                    replStr: "Cannot create a $1 key iterator for a non-$1 object."
                },
                {
                    regexp: /(Array|Map|Set|String).prototype.values: 'this' is not a \1 object/,
                    replStr: "Cannot create a $1 value iterator for a non-$1 object."
                },
                {
                    regexp: /(Map|Set) Iterator.prototype.next: 'this' is not a \1 Iterator object/,
                    replStr: "Cannot call $1Iterator.next() on a non-$1Iterator object"
                },
                {
                    regexp: /(Array) Iterator.prototype.next: 'this' is not an \1 Iterator object/,
                    replStr: "%$1IteratorPrototype%.next requires that |this| be an $1 Iterator instance"
                },
                {
                    regexp: /(Map|String) Iterator.prototype.next: 'this' is not a \1 Iterator object/,
                    replStr: "%$1IteratorPrototype%.next requires that |this| be a $1 Iterator instance"
                },
                {
                    regexp: /String.prototype\[Symbol.iterator\]: 'this' is null or undefined/,
                    replStr: "Type error" // String.prototype[Symbol.iterator] throws if this is null"
                },
                {
                    regexp: /Promise: cannot be called without the new keyword/,
                    replStr: "Cannot call a constructor without |new|"
                },
                {
                    regexp: /Promise: argument is not a Function object/,
                    replStr: "Promise constructor takes a function argument"
                },
                {
                    regexp: /Promise.prototype.then: 'this' is not a Promise object/,
                    replStr: "|this| is not a Promise"
                },
                {
                    regexp: /Promise.resolve: 'this' is not an Object/,
                    replStr: "|this| is not an object"
                },
                {
                    regexp: /Promise.reject: 'this' is not an Object/,
                    replStr: "|this| is not an object"
                },
                {
                    regexp: /Proxy trap `setPrototypeOf` returned false/,
                    replStr: "Proxy 'setPrototypeOf' returned false indicating it could not set the prototype value. The operation was expected to succeed"
                }
            ];
            for (let idx = 0; idx < jscReplacements.length; idx++) {
                if (jscReplacements[idx].regexp.test(expectedErrorMessage)) {
                    expectedErrorMessage = expectedErrorMessage.replace(jscReplacements[idx].regexp, jscReplacements[idx].replStr);
                    break;
                }
            }
            
            if (validationPart !== expectedException || (expectedErrorMessage && validationErrorMessage !== expectedErrorMessage)) {
                var expectedString = expectedException !== undefined ?
                  expectedException.toString().replace(/\n/g, "").replace(/.*function (.*)\(.*/g, "$1") :
                  "<any exception>";
                if (expectedErrorMessage) {
                    expectedString += " " + expectedErrorMessage;
                }
                var actual = exception !== noException ? exception : "<no exception>";
                throw addMessage("assert.throws failed: expected: " + expectedString + ", actual: " + actual, message);
            }
        },

        doesNotThrow: function doesNotThrow(testFunction, message) {
            var noException = {};
            var exception = noException;
            try {
                testFunction();
            } catch (ex) {
                exception = ex;
            }

            if (exception === noException) {
                return;
            }

            throw addMessage("assert.doesNotThrow failed: expected: <no exception>, actual: " + exception, message);
        },

        fail: function fail(message) {
            ///<summary>Can be used to fail the test.</summary>
            throw message;
        }
    }
}(); // assert.
