//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Deletion of a property from SimpleTypeHandler shouldn't cause other properties to be deleted",
        body: function () {
            // When a property from SimpleTypeHandler was deleted, we used to
            // convert it to NullTypeHandler (even if there are multiple
            // properties) and this caused removal of other properties from the
            // Object.

            // A script function should have a SimpleTypeHandler as its type handler.
            function f() {
            }

            delete f.name;

            var desc = Object.getOwnPropertyDescriptor(f, 'prototype');
            assert.isNotUndefined(desc);
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
