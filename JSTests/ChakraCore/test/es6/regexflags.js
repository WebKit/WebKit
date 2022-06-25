//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function bindFlagsGetter(thisArg) {
    var getter = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get;
    return getter.bind(thisArg);
}

function generatePrototypeFlagsTests() {
    var testsGroupedByFlag =
        [['global', 'g'],
         ['ignoreCase', 'i'],
         ['multiline', 'm'],
         ['sticky', 'y'],
         ['unicode', 'u']].map(function ([propertyName, flag]) {
            return [
                {
                    name: "RegExp.prototype.flags should include '" + flag + "' when '" + propertyName + "' is 'true'",
                    body: function () {
                        var object = {};
                        object[propertyName] = true;
                        var flags = bindFlagsGetter(object)();
                        assert.isTrue(flags.includes(flag));
                    }
                },
                {
                    name: "RegExp.prototype.flags should not include '" + flag + "' when '" + propertyName + "' is 'false'",
                    body: function () {
                        var object = {};
                        object[propertyName] = false;
                        var flags = bindFlagsGetter(object)();
                        assert.isFalse(flags.includes(flag));
                    }
                },
                {
                    name: "RegExp.prototype.flags should coerce '" + flag + "' to Boolean",
                    body: function () {
                        var object = {};

                        object[propertyName] = 123;
                        var flags = bindFlagsGetter(object)();
                        assert.isTrue(flags.includes(flag), "123 -> true");

                        object[propertyName] = null;
                        var flags = bindFlagsGetter(object)();
                        assert.isFalse(flags.includes(flag), "null -> false");
                    }
                },
            ];
        });
    return Array.prototype.concat.apply([], testsGroupedByFlag);
}

var tests = [
    {
        name: "Test sticky and unicode getter on RegExp.prototype",
        body: function () {
            assert.isFalse(RegExp.prototype.unicode, "RegExp.prototype.unicode");
            assert.isFalse(RegExp.prototype.sticky, "RegExp.prototype.sticky");

            function verifier(r, expectedUnicode, expectedSticky)
            {
                r.unicode = true; // no-op
                r.sticky = true; // no-op
                assert.areEqual(expectedUnicode, r.unicode, r + ": unicode");
                assert.areEqual(expectedSticky, r.sticky, r + ": sticky");
            }

            verifier(/.?\d+/, false, false);
            RegExp.prototype.unicode = true; // no-op
            verifier(/.?\d+/gu, true, false);
            verifier(/.?\d+/iy, false, true);
            RegExp.prototype.sticky = true; // no-op
            verifier(new RegExp("a*bc", "g"), false, false);
            verifier(new RegExp("a*bc", "u"), true, false);
            verifier(new RegExp("a*bc?","y"), false, true);
            verifier(new RegExp("a*b\d\s?","iuy"), true, true);
        }
    },
    {
        name: "Test sticky bit with exec()",
        body: function () {
            var pattern = /hello\d\s?/y;
            var text = "hello1 hello2 hello3";
            [["hello1 ", 7],
             ["hello2 ", 14],
             ["hello3", 20],
             [null, 0]].forEach(function ([expectedMatchedString, expectedLastIndex]) {
                var result = pattern.exec(text);
                if (expectedMatchedString !== null) {
                    assert.areEqual(expectedMatchedString, result[0], "matched string");
                }
                else {
                    assert.areEqual(null, result, "result");
                }
                assert.areEqual(expectedLastIndex, pattern.lastIndex, "lastIndex");
            });
        }
    },
    {
        name: "Test sticky bit with test()",
        body: function () {
            var pattern = /hello\d\s?/y;
            var text = "hello1 hello2 hello3";
            [[true, 7],
             [true, 14],
             [true, 20],
             [false, 0]].forEach(function ([expectedResult, expectedLastIndex]) {
                assert.areEqual(expectedResult, pattern.test(text), "result");
                assert.areEqual(expectedLastIndex, pattern.lastIndex, "lastIndex");
            });
        }
    },
    {
        name: "Test sticky bit with search()",
        body: function () {
            var pattern = /hello\d\s?/y;
            var text = "hello1 hello2 hello3";
            assert.areEqual(0, text.search(pattern), "result");
            assert.areEqual(0, pattern.lastIndex, "lastIndex")
        }
    },
    {
        name: "Test sticky bit with replace()",
        body: function () {
            var pattern = /hello\d\s?/y;
            var text = "hello1 hello2 hello3";
            // should replace 1st occurrence because global is false and last index should be at 7.
            assert.areEqual("world hello2 hello3", text.replace(pattern, "world "), "result");
            assert.areEqual(7, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "Test sticky bit with replace() with global flag.",
        body: function () {
            var pattern = /hello\d\s?/g;
            var text = "hello1 hello2 hello3";
            // should replace all occurrences because global is true and last index should be at 0.
            assert.areEqual("world world world ", text.replace(pattern, "world "), "result");
            assert.areEqual(0, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "Test sticky bit with replace() with global/sticky flag.",
        body: function () {
            var pattern = /hello\d\s?/gy;
            var text = "hello1 hello2 hello3";
            // should replace all occurrences because global is true irrespective of sticky bit and last index should be at 0.
            assert.areEqual("world world world ", text.replace(pattern, "world "), "result");
            assert.areEqual(0, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "Test sticky bit with match()",
        body: function () {
            var pattern = /hello\d\s?/y;
            var text = "hello1 hello2 hello3";
            // should match 1st occurrence because global is false and last index should be at 7.
            var result = text.match(pattern);
            assert.areEqual(1, result.length, "result length");
            assert.areEqual("hello1 ", result[0], "result[0]");
            assert.areEqual(7, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "Test sticky bit with match() with global flag.",
        body: function () {
            var pattern = /hello\d\s?/g;
            var text = "hello1 hello2 hello3";
            // should match all occurrence because global is true and last index should be at 0.
            var result = text.match(pattern);
            assert.areEqual(3, result.length, "result length");
            assert.areEqual("hello1 ", result[0], "result[0]");
            assert.areEqual("hello2 ", result[1], "result[1]");
            assert.areEqual("hello3", result[2], "result[2]");
            assert.areEqual(0, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "Test sticky bit with match() with global/sticky flag.",
        body: function () {
            var pattern = /hello\d\s?/gy;
            var text = "hello1 hello2 hello3";
            // should match all occurrence because global is true and last index should be at 0 irrespective of sticky bit flag.
            var result = text.match(pattern);
            assert.areEqual(3, result.length, "result length");
            assert.areEqual("hello1 ", result[0], "result[0]");
            assert.areEqual("hello2 ", result[1], "result[1]");
            assert.areEqual("hello3", result[2], "result[2]");
            assert.areEqual(0, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "Test sticky bit with split()",
        body: function () {
            var pattern = /\d/y;
            var text = "hello1 hello2 hello3";
            // sticky bit flag is ignored and lastIndex is set to 0.
            var result = text.split(pattern);
            assert.areEqual(4, result.length, "result length");
            assert.areEqual("hello", result[0], "result[0]");
            assert.areEqual(" hello", result[1], "result[1]");
            assert.areEqual(" hello", result[2], "result[2]");
            assert.areEqual("", result[3], "result[3]");
            assert.areEqual(0, pattern.lastIndex, "lastIndex");
        }
    },
    {
        name: "RegExp.prototype.flags should throw an exception when 'this' isn't an Object",
        body: function () {
            var nonObject = "string";
            assert.throws(bindFlagsGetter(nonObject), TypeError);
        }
    },
    {
        name: "RegExp.prototype.flags should be callable when 'this' is an ordinary Object",
        body: function () {
            assert.doesNotThrow(bindFlagsGetter({}));
        }
    },
    {
        name: "RegExp.prototype.flags should return an empty String when no flag exists",
        body: function () {
            assert.areEqual("", bindFlagsGetter({})());
        }
    },
    {
        name: "RegExp.prototype.flags should return the flags in the correct order",
        body: function () {
            var object = {
                ignoreCase: true,
                unicode: true,
                sticky: true,
                multiline: true,
                global: true
            };
            assert.areEqual("gimuy", bindFlagsGetter(object)());
        }
    },
];
tests = tests.concat(generatePrototypeFlagsTests());

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
