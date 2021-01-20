//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES7 Object Prototype object has an immutable [[Prototype]] internal slot
// See: 19.1.3 Properties of the Object Prototype Object
// See: 9.4.7 Immutable Prototype Exotic Objects

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Not okay to set Object.prototype.[[Prototype]] using __proto__",
        body: function () {
            var objectPrototypeObject = Object.getPrototypeOf(Object.prototype)
            var b = Object.create(null)

            assert.throws(function () { Object.prototype.__proto__ = b },
                TypeError,
                "It should not be okay to set Object.prototype.[[Prototype]] using __proto__",
                "Can't set the prototype of this object.")

            assert.areEqual(objectPrototypeObject, Object.prototype.__proto__, "Object.prototype.__proto__ is unchanged")
            assert.areEqual(objectPrototypeObject, Object.getPrototypeOf(Object.prototype), "Object.getPrototypeOf(Object.prototype) is unchanged")
        }
    },
    {
        name: "Not okay to set Object.prototype.[[Prototype]] using Object.setPrototypeOf",
        body: function () {
            var objectPrototypeObject = Object.getPrototypeOf(Object.prototype)
            var b = Object.create(null)

            assert.throws(function () { Object.setPrototypeOf(Object.prototype, b) },
                TypeError,
                "It should not be okay to set Object.prototype.[[Prototype]] using Object.setPrototypeOf",
                "Can't set the prototype of this object.")

            assert.areEqual(objectPrototypeObject, Object.prototype.__proto__, "Object.prototype.__proto__ is unchanged")
            assert.areEqual(objectPrototypeObject, Object.getPrototypeOf(Object.prototype), "Object.getPrototypeOf(Object.prototype) is unchanged")
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
