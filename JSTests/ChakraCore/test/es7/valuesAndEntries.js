//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Object.values/entries should exist and constructed properly",
        body: function () {
            assert.isTrue(Object.hasOwnProperty('values'), "Object should have a values method");
            assert.isTrue(Object.hasOwnProperty('entries'), "Object should have a entries method");
            assert.areEqual(1, Object.values.length, "values method takes one argument");
            assert.areEqual(1, Object.entries.length, "entries method takes one argument");
            assert.areEqual("values", Object.values.name, "values.name is 'values'");
            assert.areEqual("entries", Object.entries.name, "entries.name is 'entries'");
            
            var descriptor = Object.getOwnPropertyDescriptor(Object, 'values');
            assert.isTrue(descriptor.writable, "writable(values) must be true");
            assert.isFalse(descriptor.enumerable, "enumerable(values) must be false");
            assert.isTrue(descriptor.configurable, "configurable(values) must be true");
            
            descriptor = Object.getOwnPropertyDescriptor(Object, 'entries');
            assert.isTrue(descriptor.writable, "writable(entries) must be true");
            assert.isFalse(descriptor.enumerable, "enumerable(entries) must be false");
            assert.isTrue(descriptor.configurable, "configurable(entries) must be true");
        }
    },
    {
        name: "Object.values/entries basic syntax",
        body: function () {
            assert.throws(function () { eval("Object.values();"); }, TypeError, "Calling values method without any arguments throws an error", "Object expected");
            assert.throws(function () { eval("Object.values(undefined);"); }, TypeError, "Calling values method on undefined throws an error", "Object expected");
            assert.throws(function () { eval("Object.values(null);"); }, TypeError, "Calling values method on null throws an error", "Object expected");
            assert.isTrue(Array.isArray(Object.values({})), "calling values method on an object returns an array");
            assert.areEqual(0, Object.values({}).length, "calling values method on an empty object returns an empty array");

            assert.throws(function () { eval("Object.entries();"); }, TypeError, "Calling entries method without any arguments throws an error", "Object expected");
            assert.throws(function () { eval("Object.entries(undefined);"); }, TypeError, "Calling entries method on undefined throws an error", "Object expected");
            assert.throws(function () { eval("Object.entries(null);"); }, TypeError, "Calling entries method on null throws an error", "Object expected");
            assert.isTrue(Array.isArray(Object.entries({})), "calling entries method on an object returns an array");
            assert.areEqual(0, Object.entries({}).length, "calling entries method on an empty object returns an empty array");
        }
    },
    {
        name: "Object.values/entries functionality",
        body: function () {
            var a1 = {prop1:10, prop2:20};
            var values = Object.values(a1);
            assert.areEqual(2, values.length, "calling values on an object with two properties will returned an array of 2 elements");
            assert.areEqual(10, values[0], "First element of the returned values array should be 10");
            assert.areEqual(20, values[1], "Second element of the returned values array should be 20");

            var entries = Object.entries(a1);
            assert.areEqual(2, entries.length, "calling entries on an object with two properties will returned an array of 2 elements");
            assert.isTrue(Array.isArray(entries[0]) && Array.isArray(entries[1]), "each element itself an array of key, value pair");
            assert.areEqual(["prop1", 10], entries[0], "First element of the returned entry array should be ['prop1', 10]");
            assert.areEqual(["prop2", 20], entries[1], "Second element of the returned entry array should be ['prop2', 20]");
            
            var a2 = {prop3 : 30};
            a2[2] = 40;
            a2["prop4"] = 50;
            Object.defineProperty(a2, "prop5", { value: 60, enumerable: true});
            Object.defineProperty(a2, "prop6", { value: 70, enumerable: false});
            Object.defineProperty(a2, 'prop7', {  enumerable: true, get: function () { return 80;}});
            var sym = Symbol('prop8');
            a2[sym] = 90;

            values = Object.values(a2);
            assert.areEqual(5, values.length, "values method returns an array of 5 elements, symbol and non-enumerable should be excluded");
            assert.areEqual([40,30,50,60,80], values, "values method returns an array and matches correctly");
            
            entries = Object.entries(a2);
            assert.areEqual(5, entries.length, "entries method returns an array of 5 elements, symbol and non-enumerable should be excluded");
            assert.areEqual("2,40,prop3,30,prop4,50,prop5,60,prop7,80", entries.toString(), "entries method returns an array and matches correctly");
        }
    },
    {
        name: "Object.values/entries with proxy",
        body: function () {
            var obj1 = {prop1:10};
            var proxy1 = new Proxy(obj1, { });
            var values = Object.values(proxy1);
            assert.areEqual(1, values.length, "values - Proxy object on an object with one property returns an array of 1 element");
            assert.areEqual(10, values[0], "values - Proxy object on an object with one property returns correct element");

            var entries = Object.entries(proxy1);
            assert.areEqual(1, entries.length, "entries - Proxy object on an object with one property returns an array of 1 element");
            assert.areEqual(["prop1", 10], entries[0], "entries - Proxy object on an object with one property returns correct element");

            var obj2 = {};
            Object.defineProperty(obj2, "prop2", { value: 20, enumerable: true });
            Object.defineProperty(obj2, "prop3", { get: function () { return 30; }, enumerable: true });
            var proxy2 = new Proxy(obj2, {
                getOwnPropertyDescriptor: function (target, property) {
                    return Reflect.getOwnPropertyDescriptor(target, property);
                }
            });
            
            values = Object.values(proxy2);
            assert.areEqual(2, values.length, "values - exhibiting a Proxy trapping getOwnPropertyDescriptor returns an aray to 2 elements");
            assert.areEqual(20, values[0], "values - a Proxy trapping getOwnPropertyDescriptor matching the first element");
            assert.areEqual(30, values[1], "values - a Proxy trapping getOwnPropertyDescriptor matching the second element");

            entries = Object.entries(proxy2);
            assert.areEqual(2, entries.length, "values - exhibiting a Proxy trapping getOwnPropertyDescriptor returns an aray to 2 elements");
            assert.areEqual(["prop2", 20], entries[0], "entries - a Proxy trapping getOwnPropertyDescriptor matching the first element");
            assert.areEqual(["prop3", 30], entries[1], "entries - a Proxy trapping getOwnPropertyDescriptor matching the second element");

            var obj3 = {};
            var count = 0;
            var proxy3 = new Proxy(obj3, {
                get: function (target, property, receiver) {
                    return count++ * 5;
                },
                getOwnPropertyDescriptor: function (target, property) {
                    return {configurable: true, enumerable: true};
                },

                ownKeys: function (target) {
                    return ["prop0", "prop1", Symbol("prop2"), Symbol("prop5")];
                }
            });

            values = Object.values(proxy3);
            assert.areEqual(2, values.length, "values - exhibiting a Proxy with get and ownKeys traps - returns 2 elements");
            assert.areEqual(0, values[0], "values - exhibiting a Proxy with get and ownKeys traps - matching first element");
            assert.areEqual(5, values[1], "values - exhibiting a Proxy with get and ownKeys traps -  matching second element");

            entries = Object.entries(proxy3);
            assert.areEqual(2, entries.length, "entries - exhibiting a Proxy with get and ownKeys trap - returns 2 elements");
            assert.areEqual(["prop0", 10], entries[0], "entries - exhibiting a Proxy with get and ownKeys trap - matching first element");
            assert.areEqual(["prop1", 15], entries[1], "entries - exhibiting a Proxy with get and ownKeys trap - matching second element");

        }
    },
    {
        name: "Object.values - deleting or making non-enumerable during enumeration",
        body: function () {
            var aDeletesB = {
                get a() {
                    delete this.b;
                    return 1;
                },
                b: 2
            };
            
            var values = Object.values(aDeletesB);
            assert.areEqual(1, values.length, "deleting next key during enumeration is excluded in the result");
            assert.areEqual(1, values[0], "deleting next key - first element is 1");
            
            var aRemovesB = {
                get a() {
                    Object.defineProperty(this, 'b', {
                        enumerable: false
                    });
                    return 1;
                },
                b: 2
            };

            values = Object.values(aRemovesB);
            assert.areEqual(1, values.length, "making the next key non-enumerable is excluded in the result");
            assert.areEqual(1, values[0], "making next nonenumerable - first element is 1");
        }
    },
    {
        name: "Object.entries - deleting or making non-enumerable during enumeration",
        body: function () {
            var aDeletesB = {
                get a() {
                    delete this.b;
                    return 1;
                },
                b: 2
            };

            var entries = Object.entries(aDeletesB);
            assert.areEqual(1, entries.length, "deleting next key during enumeration is excluded in the result");
            assert.areEqual(['a', 1], entries[0], "deleting next key - first element is 1");
            
            var aRemovesB = {
                get a() {
                    Object.defineProperty(this, 'b', {
                        enumerable: false
                    });
                    return 1;
                },
                b: 2
            };

            entries = Object.entries(aRemovesB);
            assert.areEqual(1, entries.length, "making the next key non-enumerable is excluded in the result");
            assert.areEqual(['a', 1], entries[0], "making next nonenumerable - first element is 1");
        }
    },
    {
        name: "Object.values - making non-enumerable to enumerable during enumeration",
        body: function () {
            var aAddsB = {};
            Object.defineProperty(aAddsB, "a", { get: function () { Object.defineProperty(this, 'b', { enumerable: true }); return 1; }, enumerable: true });
            Object.defineProperty(aAddsB, "b", { value: 2, configurable:true, enumerable: false});
            
            var values = Object.values(aAddsB);
            assert.areEqual(2, values.length, "making the second non-enumerable key to enumerable is included in the result");
            assert.areEqual(1, values[0], "non-enumerable to enumerable - first element is 1");
            assert.areEqual(2, values[1], "non-enumerable to enumerable - second element is 2");
        }
    },
    {
        name: "Object.entries - making non-enumerable to enumerable during enumeration",
        body: function () {
            var aAddsB = {};
            Object.defineProperty(aAddsB, "a", { get: function () { Object.defineProperty(this, 'b', { enumerable:true }); return 1; }, enumerable: true });
            Object.defineProperty(aAddsB, "b", { value: 2, configurable:true, enumerable: false});
            
            var entries = Object.entries(aAddsB);
            assert.areEqual(2, entries.length, "making the second non-enumerable key to enumerable is included in the result");
            assert.areEqual(['a', 1], entries[0], "non-enumerable to enumerable - first element is 1");
            assert.areEqual(['b', 2], entries[1], "non-enumerable to enumerable - second element is 2");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
