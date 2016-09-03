//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function verifyToStringSource(re, overriddenSource, expectedSource) {
    Object.defineProperty(re, 'source', {value: overriddenSource});
    var str = re.toString();
    var [, returnedSource,] = str.split('/');
    assert.areEqual(expectedSource, returnedSource, "source");
}

function verifyToStringFlags(re, overriddenFlags, expectedFlags) {
    Object.defineProperty(re, 'flags', {value: overriddenFlags});
    var str = re.toString();
    var [, , returnedFlags] = str.split('/');
    assert.areEqual(expectedFlags, returnedFlags, "flags");
}

function flattenArray(array) {
    return Array.prototype.concat.apply([], array);
}

var flagPropertyNames = [
    "global",
    "ignoreCase",
    "multiline",
    "options",
    "sticky",
    "unicode",
];
var nonGenericPropertyNames = flagPropertyNames.concat("source");
var propertyNames = nonGenericPropertyNames.concat("flags");

var tests = flattenArray(propertyNames.map(function (name) {
    // Values returned by the properties are tested in other files since they
    // are independent of the config flag and work regardless of where the
    // properties are.
    return [
        {
            name: name + " exists on the prototype",
            body: function () {
                var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
                assert.isNotUndefined(descriptor, "descriptor");

                assert.isFalse(descriptor.enumerable, name + " is not enumerable");
                assert.isTrue(descriptor.configurable, name + " is configurable");
                assert.isUndefined(descriptor.value, name + " does not have a value");
                assert.isUndefined(descriptor.set, name + " does not have a setter");

                var getter = descriptor.get;
                assert.isNotUndefined(getter, name + " has a getter");
                assert.areEqual('function', typeof getter, "Getter for " + name + " is a function");
                assert.areEqual("get " + name, descriptor.get.name, "Getter for " + name + " has the correct name");
                assert.areEqual(0, descriptor.get.length, "Getter for " + name + " has the correct length");
            }
        },
        {
            name: name + " does not exist on the instance",
            body: function () {
                var descriptor = Object.getOwnPropertyDescriptor(/./, name);
                assert.isUndefined(descriptor, name);
            }
        },
        {
            name: name + " getter can be called by RegExp subclasses",
            body: function () {
                class Subclass extends RegExp {}
                var re = new Subclass();
                assert.doesNotThrow(function () { re[name] });
            }
        },
        {
            name: name + " should be deletable",
            body: function () {
                var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
                delete RegExp.prototype[name];
                assert.isFalse(name in RegExp.prototype);
                Object.defineProperty(RegExp.prototype, name, descriptor);
            }
        }
    ];
}));
tests = tests.concat(nonGenericPropertyNames.map(function (name) {
    return {
        name: name + " getter can not be called by non-RegExp objects",
        body: function () {
            var o = Object.create(/./);
            var getter = Object.getOwnPropertyDescriptor(RegExp.prototype, name).get;
            assert.throws(getter.bind(o));
        }
    };
}));
tests = tests.concat(flagPropertyNames.map(function (name) {
    return {
        name: name + " getter should return 'undefined' when called with RegExp prototype",
        body: function () {
            var getter = Object.getOwnPropertyDescriptor(RegExp.prototype, name).get;

            var result = getter.call(RegExp.prototype);

            assert.isUndefined(result);
        }
    };
}));
tests = tests.concat([
    {
        name: "RegExp.prototype.source getter should return '(?:)' when called with RegExp prototype",
        body: function () {
            var getter = Object.getOwnPropertyDescriptor(RegExp.prototype, "source").get;

            var result = getter.call(RegExp.prototype);

            assert.areEqual("(?:)", result);
        }
    },
    {
        name: "RegExp.prototype.toString should be generic",
        body: function () {
            var re = {
                source: "source",
                flags: "gi"
            };

            var string = RegExp.prototype.toString.call(re);

            assert.areEqual("/source/gi", string);
        }
    },
    {
        name: "RegExp.prototype.toString should use the string 'undefined' when the 'source' property is missing",
        body: function () {
            var overriddenSource = undefined;
            var expectedSource = "undefined";
            verifyToStringSource(/source/, overriddenSource, expectedSource);
        }
    },
    {
        name: "RegExp.prototype.toString should coerce the 'source' property to String",
    body: function () {
            var overriddenSource = 1;
            var expectedSource = overriddenSource.toString();
            verifyToStringSource(/source/, overriddenSource, expectedSource);
        }
    },
    {
        name: "RegExp.prototype.toString should use the 'source' property from a RegExp subclass",
        body: function () {
            class Subclass extends RegExp {}
            var overriddenSource = "source";
            var expectedSource = overriddenSource;
            verifyToStringSource(new Subclass(), overriddenSource, expectedSource);
        }
    },
    {
        name: "RegExp.prototype.toString should use the string 'undefined' when the 'flags' property is missing",
        body: function () {
            var overriddenFlags = undefined;
            var expectedFlags = "undefined";
            verifyToStringFlags(/./g, overriddenFlags, expectedFlags);
        }
    },
    {
        name: "RegExp.prototype.toString should coerce the 'flags' property to String",
        body: function () {
            var overriddenFlags = 1;
            var expectedFlags = overriddenFlags.toString();
            verifyToStringFlags(/./g, overriddenFlags, expectedFlags);
        }
    },
    {
        name: "RegExp.prototype.toString should use the 'flags' property from a RegExp subclass",
        body: function () {
            class Subclass extends RegExp {}
            var overriddenFlags = 'imy';
            var expectedFlags = overriddenFlags;
            verifyToStringFlags(new Subclass(), overriddenFlags, expectedFlags)
        }
    },
]);

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != 'summary' });
