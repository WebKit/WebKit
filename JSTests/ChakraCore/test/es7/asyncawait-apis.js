//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Async Await API tests -- verifies built-in API objects and properties

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var globObj = this;

function checkAttributes(name, o, p, a) {
    var desc = Object.getOwnPropertyDescriptor(o, p);

    var msgPrefix = "Property " + p.toString() + " on " + name + " is ";

    assert.isTrue(!!desc, msgPrefix + "not found; there is no descriptor");

    assert.areEqual(a.writable, desc.writable, msgPrefix + (a.writable ? "" : "not") + " writable");
    assert.areEqual(a.enumerable, desc.enumerable, msgPrefix + (a.enumerable ? "" : "not") + " enumerable");
    assert.areEqual(a.configurable, desc.configurable, msgPrefix + (a.configurable ? "" : "not") + " configurable");
}

var tests = [
    {
        name: "AsyncFunction is not exposed on the global object",
        body: function () {
            assert.isFalse(globObj.hasOwnProperty("AsyncFunction"), "Global object does not have property named AsyncFunction");
        }
    },
    {
        name: "Async function instances have length and name properties",
        body: function () {
            async function af() { }

            assert.isTrue(af.hasOwnProperty("length"), "Async function objects have a 'length' property");
            assert.isTrue(af.hasOwnProperty("name"), "Async function objects have a 'name' property");

            checkAttributes("af", af, "length", { writable: false, enumerable: false, configurable: true });
            checkAttributes("af", af, "name", { writable: false, enumerable: false, configurable: true });

            assert.areEqual(0, af.length, "Async function object's 'length' property matches the number of parameters (0)");
            assert.areEqual("af", af.name, "Async function object's 'name' property matches the function's name");

            async function af2(a, b, c) { }
            assert.areEqual(3, af2.length, "Async function object's 'length' property matches the number of parameters (3)");
        }
    },
    {
        name: "Async functions are not constructors and do not have a prototype property",
        body: function () {
            async function af() { }

            assert.throws(function () { new af(); }, TypeError, "Async functions cannot be used as constructors", "Function is not a constructor");
            assert.isFalse(af.hasOwnProperty("prototype"), "Async function objects do not have a 'prototype' property");
        }
    },
    {
        name: "Async functions do not have arguments nor caller properties regardless of strictness",
        body: function () {
            async function af() { }

            assert.isFalse(af.hasOwnProperty("arguments"), "Async function objects do not have an 'arguments' property");
            assert.isFalse(af.hasOwnProperty("caller"), "Async function objects do not have a 'caller' property");

            // Test JavascriptFunction APIs that special case PropertyIds::caller and ::arguments
            Object.setPrototypeOf(af, Object.prototype); // Remove Function.prototype so we don't find its 'caller' and 'arguments' in these operations
            assert.isFalse("arguments" in af, "Has operation on 'arguments' property returns false initially");
            assert.areEqual(undefined, af.arguments, "Get operation on 'arguments' property returns undefined initially");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(af, "arguments"), "No property descriptor for 'arguments' initially");
            assert.isTrue(delete af.arguments, "Delete operation on 'arguments' property returns true");

            assert.areEqual(0, af.arguments = 0, "Set operation on 'arguments' creates new property with assigned value");
            assert.isTrue("arguments" in af, "Has operation on 'arguments' property returns true now");
            assert.areEqual(0, af.arguments, "Get operation on 'arguments' property returns property value now");
            checkAttributes("af", af, "arguments", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete af.arguments, "Delete operation on 'arguments' property still returns true");
            assert.isFalse(af.hasOwnProperty("arguments"), "'arguments' property is gone");

            assert.isFalse("caller" in af, "Has operation on 'caller' property returns false initially");
            assert.areEqual(undefined, af.caller, "Get operation on 'caller' property returns undefined initially");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(af, "caller"), "No property descriptor for 'caller' initially");
            assert.isTrue(delete af.caller, "Delete operation on 'caller' property returns true");

            assert.areEqual(0, af.caller = 0, "Set operation on 'caller' creates new property with assigned value");
            assert.isTrue("caller" in af, "Has operation on 'caller' property returns true now");
            assert.areEqual(0, af.caller, "Get operation on 'caller' property returns property value now");
            checkAttributes("af", af, "caller", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete af.caller, "Delete operation on 'caller' property still returns true");
            assert.isFalse(af.hasOwnProperty("caller"), "'caller' property is gone");

            async function afstrict() { "use strict"; }

            assert.isFalse(afstrict.hasOwnProperty("arguments"), "Strict mode async function objects do not have an 'arguments' property");
            assert.isFalse(afstrict.hasOwnProperty("caller"), "Strict mode async function objects do not have a 'caller' property");

            Object.setPrototypeOf(afstrict, Object.prototype); // Remove Function.prototype so we don't find its 'caller' and 'arguments' in these operations
            assert.isFalse("arguments" in afstrict, "Has operation on 'arguments' property returns false initially for a strict mode async function");
            assert.areEqual(undefined, afstrict.arguments, "Get operation on 'arguments' property returns undefined initially for a strict mode async function");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(afstrict, "arguments"), "No property descriptor for 'arguments' initially for a strict mode async function");
            assert.isTrue(delete afstrict.arguments, "Delete operation on 'arguments' property returns true initially for a strict mode async function");

            assert.areEqual(0, afstrict.arguments = 0, "Set operation on 'arguments' creates new property with assigned value for a strict mode async function");
            assert.isTrue("arguments" in afstrict, "Has operation on 'arguments' property returns true now for a strict mode async function");
            assert.areEqual(0, afstrict.arguments, "Get operation on 'arguments' property returns property value now for a strict mode async function");
            checkAttributes("afstrict", afstrict, "arguments", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete afstrict.arguments, "Delete operation on 'arguments' property still returns true for a strict mode async function");
            assert.isFalse(afstrict.hasOwnProperty("arguments"), "'arguments' property is gone for a strict mode async function");

            assert.isFalse("caller" in afstrict, "Has operation on 'caller' property returns false initially for a strict mode async function");
            assert.areEqual(undefined, afstrict.caller, "Get operation on 'caller' property returns undefined initially for a strict mode async function");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(afstrict, "caller"), "No property descriptor for 'caller' initially for a strict mode async function");
            assert.isTrue(delete afstrict.caller, "Delete operation on 'caller' property returns true initially for a strict mode async function");

            assert.areEqual(0, afstrict.caller = 0, "Set operation on 'caller' creates new property with assigned value for a strict mode async function");
            assert.isTrue("caller" in afstrict, "Has operation on 'caller' property returns true now for a strict mode async function");
            assert.areEqual(0, afstrict.caller, "Get operation on 'caller' property returns property value now for a strict mode async function");
            checkAttributes("afstrict", afstrict, "caller", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete afstrict.caller, "Delete operation on 'caller' property still returns true for a strict mode async function");
            assert.isFalse(afstrict.hasOwnProperty("caller"), "'caller' property is gone for a strict mode async function");
        }
    },
    {
        name: "Async function instances have %AsyncFunctionPrototype% as their prototype and it has the specifies properties and prototype",
        body: function () {
            async function af() { }
            var asyncFunctionPrototype = Object.getPrototypeOf(af);

            assert.areEqual(Function.prototype, Object.getPrototypeOf(asyncFunctionPrototype), "%AsyncFunctionPrototype%'s prototype is Function.prototype");

            assert.isTrue(asyncFunctionPrototype.hasOwnProperty("constructor"), "%AsyncFunctionPrototype% has 'constructor' property");
            assert.isTrue(asyncFunctionPrototype.hasOwnProperty(Symbol.toStringTag), "%AsyncFunctionPrototype% has [Symbol.toStringTag] property");

            checkAttributes("%AsyncFunctionPrototype%", asyncFunctionPrototype, "constructor", { writable: false, enumerable: false, configurable: true });
            checkAttributes("%AsyncFunctionPrototype%", asyncFunctionPrototype, Symbol.toStringTag, { writable: false, enumerable: false, configurable: true });

            assert.areEqual("AsyncFunction", asyncFunctionPrototype[Symbol.toStringTag], "%AsyncFunctionPrototype%'s [Symbol.toStringTag] property is 'AsyncFunction'");

            assert.isFalse(asyncFunctionPrototype.hasOwnProperty("prototype"), "%AsyncFunctionPrototype% does not have a 'prototype' property");
        }
    },
    {
        name: "%AsyncFunction% constructor is the value of the constructor property of %AsyncFunctionPrototype%.prototype and has the specified properties and prototype",
        body: function () {
            async function af() { }
            var asyncFunctionPrototype = Object.getPrototypeOf(af);
            var asyncFunctionConstructor = asyncFunctionPrototype.constructor;

            assert.areEqual(Function, Object.getPrototypeOf(asyncFunctionConstructor), "%AsyncFunction%'s prototype is Function");

            assert.isTrue(asyncFunctionConstructor.hasOwnProperty("name"), "%AsyncFunction% has 'name' property");
            assert.isTrue(asyncFunctionConstructor.hasOwnProperty("length"), "%AsyncFunction% has 'length' property");
            assert.isTrue(asyncFunctionConstructor.hasOwnProperty("prototype"), "%AsyncFunction% has 'prototype' property");

            checkAttributes("%AsyncFunction%", asyncFunctionConstructor, "name", { writable: false, enumerable: false, configurable: true });
            checkAttributes("%AsyncFunction%", asyncFunctionConstructor, "length", { writable: false, enumerable: false, configurable: true });
            checkAttributes("%AsyncFunction%", asyncFunctionConstructor, "prototype", { writable: false, enumerable: false, configurable: false });

            assert.areEqual("AsyncFunction", asyncFunctionConstructor.name, "%AsyncFunction%'s 'name' property is 'AsyncFunction'");
            assert.areEqual(asyncFunctionPrototype, asyncFunctionConstructor.prototype, "%AsyncFunction%'s 'prototype' property is %AsyncFunction%.prototype");
            assert.areEqual(1, asyncFunctionConstructor.length, "%AsyncFunction%'s 'length' property is 1");
        }
    },
    {
        name: "",
        body: function () {
            var asyncFunctionPrototype = Object.getPrototypeOf(async function () { });
            var AsyncFunction = asyncFunctionPrototype.constructor;

            var af = new AsyncFunction('return await 1;');

            assert.areEqual(asyncFunctionPrototype, Object.getPrototypeOf(af), "Async function created by %AsyncFunction% should have the same prototype as syntax declared async functions");

            assert.areEqual("anonymous", af.name, "AsyncFunction constructed async function's name is 'anonymous'");
            assert.areEqual("async function anonymous(\n) {return await 1;\n}", af.toString(), "toString of AsyncFunction constructed function is named 'anonymous'");

            af = new AsyncFunction('a', 'b', 'c', 'await a; await b; await c;');
            assert.areEqual("async function anonymous(a,b,c\n) {await a; await b; await c;\n}", af.toString(), "toString of AsyncFunction constructed function is named 'anonymous' with specified parameters");

            // Cannot verify behavior of async functions in conjunction with UnitTestFramework.js
            // due to callback nature of their execution. Instead, verification of behavior is
            // found in async-functionality.js test.
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
