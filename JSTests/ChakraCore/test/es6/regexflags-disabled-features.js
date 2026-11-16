//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests =
    [['sticky', 'y'],
     ['unicode', 'u']].map(function ([propertyName, flag]) {
        return {
            name: "RegExp.prototype.flags should not include the '" + propertyName + "' flag when the feature is disabled",
            body: function () {
                var object = {};
                object[propertyName] = true;
                var getFlags = Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get;
                var flags = getFlags.call(object);
                assert.isFalse(flags.includes(flag));
            }
        };
     });

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
