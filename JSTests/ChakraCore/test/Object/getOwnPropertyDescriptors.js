//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

function verifyObjectDescriptors(descriptors, allTruePropName, allFalsePropName) {
    var allProperties = Object.getOwnPropertyNames(descriptors).concat(Object.getOwnPropertySymbols(descriptors));

    assert.areEqual([allTruePropName, allFalsePropName], allProperties, "Result should have one descriptor for each own property");

    assert.isTrue(descriptors.hasOwnProperty(allTruePropName), "Result should contain all own properties");
    assert.isTrue(descriptors.hasOwnProperty(allFalsePropName), "Result should contain all own properties");
    assert.areEqual(descriptors[allTruePropName].value, "fooAllTrue", "Result value attribute should match the value set by defineProperties");
    assert.areEqual(descriptors[allFalsePropName].value, "fooAllFalse", "Result value attribute should match the value set by defineProperties");

    var expectedProps = ['configurable', 'writable', 'enumerable'];
    for (var i in expectedProps) {
        assert.isTrue(descriptors[allTruePropName][expectedProps[i]], "Result value attribute should match the value set by defineProperties");
        assert.isFalse(descriptors[allFalsePropName][expectedProps[i]], "Result value attribute should match the value set by defineProperties");
    }
}

var tests = [
    {
        name: "Object has getOwnPropertyDescriptors method",
            body: function() {
                assert.isTrue(Object.hasOwnProperty("getOwnPropertyDescriptors"), 'Object should have getOwnPropertyDescriptors method');

                assert.isFalse(Object.hasOwnProperty({}, "getOwnPropertyDescriptors"), 'New objects should have a property getOwnPropertyDescriptors');
                assert.isUndefined(Object.getOwnPropertyDescriptor({}, "getOwnPropertyDescriptors"), 'Object.getOwnPropertyDescriptor({}, "getOwnPropertyDescriptors") should be undefined');

                for (var p in {}) {
                    assert.isTrue(p != "getOwnPropertyDescriptors", "getOwnPropertyDescriptors should not be enumerable on new objects");
                }

                assert.areEqual(1, Object.getOwnPropertyDescriptors.length, "Object.getOwnPropertyDescriptors requires exactly one parameter.");
            }
    },
    {
        name: "Correctly handles bad parameters.",
        body: function() {
            assert.throws(function() {
                Object.getOwnPropertyDescriptors();
            }, TypeError, "Missing first parameter should cause a TypeError.", "Object expected");

            assert.throws(function() {
                Object.getOwnPropertyDescriptors(null);
            }, TypeError, "Null first parameter should cause a TypeError", "Object expected");
        }
    },
    {
        name: "The resulting get and set are identical with the original get and set.",
        body: function() {
            // This test is adapted from https://github.com/tc39/proposal-object-getownpropertydescriptors/blob/master/test/built-ins/Object/getOwnPropertyDescriptors/has-accessors.js
            var a = {
                get a() {},
                set a(value) {}
            };
            var b = Object.getOwnPropertyDescriptors(a);

            assert.isTrue(b.a.get === Object.getOwnPropertyDescriptor(a, 'a').get);
            assert.isTrue(b.a.set === Object.getOwnPropertyDescriptor(a, 'a').set);
        }
    },
    {
        name: "For properties with string names, the list of property descriptors includes all own properties with correct descriptors",
        body: function() {
            var foo = {}

            Object.defineProperties(foo, {
                "fooAllTrue": {
                    configurable: true,
                    enumerable: true,
                    value: "fooAllTrue",
                    writable: true
                },
                "fooAllFalse": {
                    configurable: false,
                    enumerable: false,
                    value: "fooAllFalse",
                    writable: false
                }
            });

            var desc = Object.getOwnPropertyDescriptors(foo);
            assert.isTrue(desc instanceof Object, "Result must be an object");

            verifyObjectDescriptors(desc, "fooAllTrue", "fooAllFalse");
        }
    },
    {
        name: "For properties with number names, the list of property descriptors includes all own properties with correct descriptors",
        body: function() {
            var foo = {}

            var allTrueNum = 1234;
            var allFalseNum = 5678;

            Object.defineProperties(foo, {
                [allTrueNum]: {
                    configurable: true,
                    enumerable: true,
                    value: "fooAllTrue",
                    writable: true
                },
                [allFalseNum]: {
                    configurable: false,
                    enumerable: false,
                    value: "fooAllFalse",
                    writable: false
                }
            });

            var desc = Object.getOwnPropertyDescriptors(foo);
            assert.isTrue(desc instanceof Object, "Result must be an object");

            verifyObjectDescriptors(desc, allTrueNum.toString(), allFalseNum.toString());

            // Also verify that the properties are accessible as numbers
            assert.areEqual(desc[allTrueNum].value, "fooAllTrue", "For properties with number names, resulting properties should be accessible with numeric names.")
            assert.areEqual(desc[allFalseNum].value, "fooAllFalse", "For properties with number names, resulting properties should be accessible with numeric names.")
        }
    },
    {
        name: "For properties with symbol names, the list of property descriptors includes all own properties with correct descriptors",
        body: function() {
            var foo = {}

            var allTrueSymbol = Symbol("allTrue");
            var allFalseSymbol = Symbol("allFalse");

            Object.defineProperties(foo, {
                [allTrueSymbol]: {
                    configurable: true,
                    enumerable: true,
                    value: "fooAllTrue",
                    writable: true
                },
                [allFalseSymbol]: {
                    configurable: false,
                    enumerable: false,
                    value: "fooAllFalse",
                    writable: false
                }
            });

            var desc = Object.getOwnPropertyDescriptors(foo);
            assert.isTrue(desc instanceof Object, "Result must be an object");

            verifyObjectDescriptors(desc, allTrueSymbol, allFalseSymbol);
        }
    },
    {
        name:"For any property, if getOwnPropertyDescriptor(property) is undefined, that property should not be present on the result.",
        body: function() {
            // Adapted from: https://github.com/ljharb/test262/blob/c2eaa30b08fb1e041b7297e415b6bad8461f50dc/test/built-ins/Object/getOwnPropertyDescriptors/proxy-undefined-descriptor.js
            var proxyHandler = {
              getOwnPropertyDescriptor: function () {},
            };

            var key = "a";
            var obj = {};
            obj[key] = "value";

            var proxy = new Proxy(obj, proxyHandler);

            var descriptor = Object.getOwnPropertyDescriptor(proxy, key);
            assert.areEqual(undefined, descriptor, "Descriptor matches result of [[GetOwnPropertyDescriptor]] trap");

            var result = Object.getOwnPropertyDescriptors(proxy);
            assert.isFalse(result.hasOwnProperty(key), "key should not be present in result");

        }
    },
]

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
