//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "String.prototype.padStart/padEnd should exist and constructed properly",
        body: function () {
            assert.isTrue(String.prototype.hasOwnProperty('padStart'), "String.prototype should have a padStart method");
            assert.isTrue(String.prototype.hasOwnProperty('padEnd'), "String.prototype should have a padEnd method");
            assert.areEqual(1, String.prototype.padStart.length, "padStart method takes one argument");
            assert.areEqual(1, String.prototype.padEnd.length, "padEnd method takes one argument");
            assert.areEqual("padStart", String.prototype.padStart.name, "padStart.name is 'padStart'");
            assert.areEqual("padEnd", String.prototype.padEnd.name, "padEnd.name is 'padEnd'");
            
            var descriptor = Object.getOwnPropertyDescriptor(String.prototype, 'padStart');
            assert.isTrue(descriptor.writable, "writable(padStart) must be true");
            assert.isFalse(descriptor.enumerable, "enumerable(padStart) must be false");
            assert.isTrue(descriptor.configurable, "configurable(padStart) must be true");
            
            descriptor = Object.getOwnPropertyDescriptor(String.prototype, 'padEnd');
            assert.isTrue(descriptor.writable, "writable(padEnd) must be true");
            assert.isFalse(descriptor.enumerable, "enumerable(padEnd) must be false");
            assert.isTrue(descriptor.configurable, "configurable(padEnd) must be true");
        }
    },
    {
        name: "String.prototype.padStart functionality",
        body: function () {
            assert.areEqual('foo'.padStart(), 'foo', "No arguments to padStart will not affect string");
            assert.areEqual('foo'.padStart(1), 'foo', "No padding added if maxLength (first argument) is less than the length of actual string");
            assert.areEqual('foo'.padStart(-1), 'foo', "No padding added if maxLength (first argument), negative, is less than the length of actual string");
            assert.areEqual('foo'.padStart(3), 'foo', "No padding added if maxLength (first argument) is equal to the length of actual string");
            assert.areEqual('foo'.padStart(4), ' foo', "String with one ' ' (SPACE) as pad is returned");
            assert.areEqual('foo'.padStart(10), '       foo', "String of length 10, with spaces filled as padding, is returned");
            assert.areEqual('foo'.padStart(10, ''), 'foo', "No padding added if the fillString is empty string");
            assert.areEqual('foo'.padStart(10, undefined), '       foo', "'undefined' fillString - string of length 10, with spaces filled as padding, is returned");
            assert.areEqual('foo'.padStart(10, ' '), '       foo', "fillString as one space - string of length 10, with spaces filled as padding, is returned");
            assert.areEqual('foo'.padStart(4, '123'), '1foo', "String of length 4, with only one character from fillString added as a padding, is returned");
            assert.areEqual('foo'.padStart(10, '123'), '1231231foo', "String of length 10, with repeatedly adding characters from fillString to create enough padding, is returned");
        }
    },
    {
        name: "String.prototype.padEnd functionality",
        body: function () {
            assert.areEqual('foo'.padEnd(), 'foo', "No arguments to padEnd will not affect string");
            assert.areEqual('foo'.padEnd(1), 'foo', "No padding added if maxLength (first argument) is less than the length of actual string");
            assert.areEqual('foo'.padEnd(-1), 'foo', "No padding added if maxLength (first argument), negative, is less than the length of actual string");
            assert.areEqual('foo'.padEnd(3), 'foo', "No padding added if maxLength (first argument) is equal to the length of actual string");
            assert.areEqual('foo'.padEnd(4), 'foo ', "String with one ' ' (SPACE) as pad is returned");
            assert.areEqual('foo'.padEnd(10), 'foo       ', "String of length 10, with spaces filled as padding, is returned");
            assert.areEqual('foo'.padEnd(10, ''), 'foo', "No padding added if the fillString is empty string");
            assert.areEqual('foo'.padEnd(10, undefined), 'foo       ', "'undefined' fillString - string of length 10, with spaces filled as padding, is returned");
            assert.areEqual('foo'.padEnd(10, ' '), 'foo       ', "fillString as one space - string of length 10, with spaces filled as padding, is returned");
            assert.areEqual('foo'.padEnd(4, '123'), 'foo1', "String of length 4, with only one character from fillString added as a padding, is returned");
            assert.areEqual('foo'.padEnd(10, '123'), 'foo1231231', "String of length 10, with repeatedly adding characters from fillString to create enough padding, is returned");
        }
    },
    {
        name: "String.prototype.padStart OOM scenario",
        body: function () {
            try {
                'foo'.padStart(2147483647);
            }
            catch(e) {
               assert.areEqual(e.message, "Out of memory", "validating out of memory for maxLength >= int_max"); 
            }
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
