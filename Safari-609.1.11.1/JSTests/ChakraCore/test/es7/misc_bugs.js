//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Inner function of 'async' function has default - should not throw any assert",
        body: function () {
            assert.doesNotThrow(function () { eval(`async function f1() {
                   function foo(a = function() { } ) { }; 
            }`); });

            assert.doesNotThrow(function () { eval(`var f1 = async ( ) => {
                   function foo(a = function() { } ) { } };`); });
                   
            assert.doesNotThrow(function () { eval(`async function f1() {
                        function foo() {
                            async function f2() {
                                function bar (a = function () {} ) {
                                }
                            }
                        }
                    }`); });
        }
    },

];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
