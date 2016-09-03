//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) {
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

var tests = [
    {
        name: "'let' should not be an allowed name in let declarations",
        body: function() {
            assert.throws(function () { eval("let test = 2, let = 1;"), SyntaxError, "'let' is not an allowed identifier in lexical declarations" });
        }
    },
    {
        name: "'let' should not be an allowed name in const declarations",
        body: function () {
            assert.throws(function () { eval("const let = 1, test = 2;"), SyntaxError, "'let' is not an allowed identifier in lexical declarations" });
        }
    },
    {
        name: "'let' should not be an allowed name in destructuring let declarations",
        body: function () {
            assert.throws(function () { eval("let [a, let, b] = [1, 2, 3];"), SyntaxError, "'let' is not an allowed identifier in lexical declarations" });
        }
    },
    {
        name: "'let' should not be an allowed name in destructuring const declarations",
        body: function () {
            assert.throws(function () { eval("const [a, let, b] = [1, 2, 3];"), SyntaxError, "'let' is not an allowed identifier in lexical declarations" });
        }
    },
    {
        name: "'let' should not be an allowed name in 'for(let .. in ..)' declarations",
        body: function () {
            assert.throws(function () { eval("for(let let in { }) { };"), SyntaxError, "'let' is not an allowed identifier in lexical declarations" });
        }
    },
    {
        name: "'let' is OK if used in var declarations",
        body: function () { var let = 1, test = 2; }
    },
]

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
