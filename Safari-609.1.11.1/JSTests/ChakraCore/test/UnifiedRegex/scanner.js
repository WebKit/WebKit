//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "SyncToLiteralsAndBackupInst should continue scanning for a literal from the furthest point scanned in the previous iteration",
        body: function () {
            var re = /<(foo|bar)/;
            var string = "0bar1<1<foo";

            // We first find "foo" at index 8, but then find "bar" at index 1.
            // Since the index of the "bar" is lower, we try to match at that
            // position. However, the "bar" isn't preceded by a '<', so we
            // retry again. In the second iteration, we're supposed to match
            // the string at index 7, right before the "foo".
            var result = re.exec(string);

            assert.areNotEqual(null, result, "result");
            assert.areEqual(7, result.index, "result.index");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != 'summary' });
