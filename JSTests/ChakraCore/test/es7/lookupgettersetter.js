//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Object.prototype.__lookupGetter__ -> [[GetOwnProperty]], [[GetPrototypeOf]]",
        body: function () {
            // Object.prototype.__lookupGetter__ -> [[GetOwnProperty]]
            // Object.prototype.__lookupGetter__ -> [[GetPrototypeOf]]
            var gopd = [];
            var gpo = false;
            var p = new Proxy({}, 
            {
                getPrototypeOf: function(o) { gpo = true; return Object.getPrototypeOf(o); },
                getOwnPropertyDescriptor: function(o, v) { gopd.push(v); return Object.getOwnPropertyDescriptor(o, v); }
            });
            Object.prototype.__lookupGetter__.call(p, "foo");
            assert.areEqual(1, gopd.length, "getOwnPropertyDescriptor should only be called once");
            assert.areEqual("foo", gopd[0], "getOwnPropertyDescriptor should be called with foo");
            assert.isTrue(gpo, "getPrototypeOf should be called");
        }
    },
    {
        name: "Object.prototype.__lookupSetter__ -> [[GetOwnProperty]], [[GetPrototypeOf]]",
        body: function () {
            // Object.prototype.__lookupSetter__ -> [[GetOwnProperty]]
            // Object.prototype.__lookupSetter__ -> [[GetPrototypeOf]]
            var gopd = [];
            var gpo = false;
            var p = new Proxy({}, 
            {
                getPrototypeOf: function(o) { gpo = true; return Object.getPrototypeOf(o); },
                getOwnPropertyDescriptor: function(o, v) { gopd.push(v); return Object.getOwnPropertyDescriptor(o, v); }
            });
            Object.prototype.__lookupSetter__.call(p, "foo");
            assert.areEqual(1, gopd.length, "getOwnPropertyDescriptor should only be called once");
            assert.areEqual("foo", gopd[0], "getOwnPropertyDescriptor should be called with foo");
            assert.isTrue(gpo, "getPrototypeOf should be called");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
