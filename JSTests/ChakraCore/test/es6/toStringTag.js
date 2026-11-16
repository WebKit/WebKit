//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var primitives = [
    { tag: "Undefined",         instance: undefined },
    { tag: "Null",              instance: null }
];

var explicitTagExoticBuiltIns = [
    { tag: "Math",              instance: Math,                                  prototype: Math },
    { tag: "JSON",              instance: JSON,                                  prototype: JSON }
];

var explicitTagExtensibleBuiltIns = [
    { tag: "Map",               instance: new Map(),                             prototype: Map.prototype },
    { tag: "Set",               instance: new Set(),                             prototype: Set.prototype },
    { tag: "WeakMap",           instance: new WeakMap(),                         prototype: WeakMap.prototype },
    { tag: "WeakSet",           instance: new WeakSet(),                         prototype: WeakSet.prototype },
    { tag: "ArrayBuffer",       instance: new ArrayBuffer(),                     prototype: ArrayBuffer.prototype },
    { tag: "DataView",          instance: new DataView(new ArrayBuffer()),       prototype: DataView.prototype }
];

var explicitTagOtherBuiltIns = [
    { tag: "Promise",           instance: new Promise(() => {}),                 prototype: Promise.prototype },
    { tag: "GeneratorFunction", instance: function* () { },                      prototype: Object.getPrototypeOf(function* () { }) },
    { tag: "Generator",         instance: (function* () { })(),                  prototype: Object.getPrototypeOf((function* () { }).prototype) },
    { tag: "String Iterator",   instance: ""[Symbol.iterator](),                 prototype: Object.getPrototypeOf(""[Symbol.iterator]()) },
    { tag: "Array Iterator",    instance: [][Symbol.iterator](),                 prototype: Object.getPrototypeOf([][Symbol.iterator]()) },
    { tag: "Map Iterator",      instance: (new Map())[Symbol.iterator](),        prototype: Object.getPrototypeOf((new Map())[Symbol.iterator]()) },
    { tag: "Set Iterator",      instance: (new Set())[Symbol.iterator](),        prototype: Object.getPrototypeOf((new Set())[Symbol.iterator]()) }
];

var explicitTagBuiltIns = [...explicitTagExoticBuiltIns, ...explicitTagExtensibleBuiltIns, ...explicitTagOtherBuiltIns];

// 22.2 TypedArray Objects Table 50: The TypedArray Constructors
var explicitTagTypedArrayBuiltIns = [
    { tag: 'Int8Array',         instance: new Int8Array(),                       prototype: Object.getPrototypeOf(Int8Array).prototype },
    { tag: 'Uint8Array',        instance: new Uint8Array(),                      prototype: Object.getPrototypeOf(Uint8Array).prototype },
    { tag: 'Uint8ClampedArray', instance: new Uint8ClampedArray(),               prototype: Object.getPrototypeOf(Uint8ClampedArray).prototype },
    { tag: 'Int16Array',        instance: new Int16Array(),                      prototype: Object.getPrototypeOf(Int16Array).prototype },
    { tag: 'Uint16Array',       instance: new Uint16Array(),                     prototype: Object.getPrototypeOf(Uint16Array).prototype },
    { tag: 'Int32Array',        instance: new Int32Array(),                      prototype: Object.getPrototypeOf(Int32Array).prototype },
    { tag: 'Uint32Array',       instance: new Uint32Array(),                     prototype: Object.getPrototypeOf(Uint32Array).prototype },
    { tag: 'Float32Array',      instance: new Float32Array(),                    prototype: Object.getPrototypeOf(Float32Array).prototype },
    { tag: 'Float64Array',      instance: new Float64Array(),                    prototype: Object.getPrototypeOf(Float64Array).prototype }
];

// A subset of builtins use the presence of an internal slot rather than a default @@toStringTag value.
// ES2017 19.1.3.6 Object.                                                       prototype.toString ( )
// 5. If isArray is true, let builtinTag be "Array".
// ...
// 14. Else, let builtinTag be "Object"
var arrayInternalSlotBuiltIn = { tag: "Array", instance: [], prototype: Array.prototype };
var internalSlotBuiltInsNoArray = [
    { tag: "String",            instance: new String(),                          prototype: String.prototype },
    { tag: "Function",          instance: function () {},                        prototype: Function.prototype },
    { tag: "Error",             instance: new Error(),                           prototype: Error.prototype },
    { tag: "Boolean",           instance: new Boolean(),                         prototype: Boolean.prototype },
    { tag: "Number",            instance: new Number(),                          prototype: Number.prototype },
    { tag: "Date",              instance: new Date(),                            prototype: Date.prototype },
    { tag: "RegExp",            instance: /a/,                                   prototype: RegExp.prototype },
    { tag: "Object",            instance: {},                                    prototype: Object.prototype },
];

var internalSlotBuiltIns = [...internalSlotBuiltInsNoArray, arrayInternalSlotBuiltIn];

var argumentsInternalSlotBuiltIn =
    { tag: "Arguments",         instance: (function () { return arguments; })(), prototype: (function () { return arguments; })() }

var tests = [
   {
       name: "Built-in prototype @@toStringTag properties",
       body: function () {
            assert.areEqual("Symbol", Symbol.prototype[Symbol.toStringTag], "check String is Symbol");
            var o  = Object.getOwnPropertyDescriptor(Symbol.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Symbol @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Symbol @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Symbol @@toStringTag is configurable");

            function checkBuiltInPrototype(builtInName, prototype) {
                // @@toStringTag default value
                assert.areEqual(builtInName, prototype[Symbol.toStringTag], builtInName + "[Symbol.toStringTag] defaults to '" + builtInName + "'");
                assert.areEqual("string", typeof prototype[Symbol.toStringTag], builtInName + "[Symbol.toStringTag] is a String")

                // @@toStringTag property descriptor
                var o  = Object.getOwnPropertyDescriptor(prototype, Symbol.toStringTag);
                assert.isFalse(o.writable,     builtInName + "[@@toStringTag] is not writable");
                assert.isFalse(o.enumerable,   builtInName + "[@@toStringTag] is not enumerable");
                assert.isTrue(o.configurable,  builtInName + "[@@toStringTag] is configurable");
            }

            // The following built-ins have an explicit @@toStringTag property on their prototype
            for (var { tag, prototype } of explicitTagBuiltIns) {
                checkBuiltInPrototype(tag, prototype);
            }
       }
    },
    {
        name: "Built-ins with no @@toStringTag property that have a default tag",
        body: function () {
            // Verify there is no @@toStringTag property.
            for (var { tag, prototype } of [...internalSlotBuiltIns, argumentsInternalSlotBuiltIn]) {
                assert.isFalse(prototype.hasOwnProperty(Symbol.toStringTag), tag + " does not have a @@toStringTag property");
            }
        }
    },
    {
        name: "Default @@toStringTag built ins",
        body: function () {
            for (var { tag, instance } of [...primitives, ...explicitTagBuiltIns, ...explicitTagTypedArrayBuiltIns, ...internalSlotBuiltIns, argumentsInternalSlotBuiltIn]) {
                assert.areEqual("[object "+ tag + "]", Object.prototype.toString.call(instance), tag + " toString uses default tag");
            }
        }
    },
    {
        name: "Proxy forwarding behavior",
        body: function () {
            function testBuiltIn({ tag, instance }, expectedTag) {
                assert.areEqual("[object " + expectedTag + "]", Object.prototype.toString.call(new Proxy(instance, {})),                tag + " proxy toString should have tag " + expectedTag);
                assert.areEqual("[object " + expectedTag + "]", Object.prototype.toString.call(new Proxy(new Proxy(instance, {}), {})), tag + " chained proxy toString should have tag " + expectedTag);
            }

            for (var row of [...explicitTagBuiltIns, arrayInternalSlotBuiltIn]) {
                testBuiltIn(row, row['tag']);
            }

            for (var row of internalSlotBuiltInsNoArray) {
                testBuiltIn(row, "Object");
            }
        }
    },
    {
       name: "Subclass @@toStringTag override of internal slot based default tag",
       body: function () {
           function testSubclassedBuiltInOverride(builtInName, instance) {
               var c = class extends eval(builtInName) {};
               assert.areEqual("[object " + builtInName + "]", Object.prototype.toString.call(new c), builtInName + " subclass toString uses default internal slot based tag before overriding");

               var instance = new c();

               var newName = builtInName + "Override";
               c[Symbol.toStringTag] = newName;
               assert.areEqual("[object " + newName + "]",     Object.prototype.toString.call(c),        builtInName + " toString uses overridden @@toStringTag after assignment");
               assert.areEqual("[object " + builtInName + "]", Object.prototype.toString.call(instance), builtInName + " instance created before override does not pick up overridden class @@toStringTag");

               var newInstanceName = builtInName + "Instance";
               instance[Symbol.toStringTag] = newInstanceName;
               assert.areEqual("[object " + newInstanceName + "]", Object.prototype.toString.call(instance), builtInName + " instance picks up @@toStringTag over the internal slot based default tag");
           }

           for (var { tag, instance } of internalSlotBuiltIns) {
               testSubclassedBuiltInOverride(tag, instance);
           }
       }
    },
    {
        name: "TypedArray @@toStringTag behavior",
        body: function () {
            function checkGetterOnPrototype(builtInName, prototype) {
                // This property has the attributes { [[Enumerable]]: false, [[Configurable]]: true }.
                var o = Object.getOwnPropertyDescriptor(prototype, Symbol.toStringTag);
                assert.isFalse(o.enumerable,  builtInName + " symbol @@toStringTag is not enumerable");
                assert.isTrue(o.configurable, builtInName + " symbol @@toStringTag is configurable");
                assert.areEqual("function", typeof o.get, builtInName + " @@toStringtag is a getter function");
            }

            for (var { tag, prototype } of explicitTagTypedArrayBuiltIns) {
                checkGetterOnPrototype(tag, prototype);
            }
        }
    },
    {
       name: "Invalid @@toStringtag (non-String coercible)",
       body: function () {
            function testBuiltIn(builtInName) {
                var c, i;
                if (builtInName == "DataView") {
                    c = class extends DataView {};
                    i = new c(new ArrayBuffer());
                } else {
                    c = class extends eval(builtInName) {};
                    i = new c();
                }
                i[Symbol.toStringTag] = null;

                assert.areEqual("[object " + builtInName + "]", Object.prototype.toString.call(i), builtInName + " defaults to internal slot based tag when @@toStringTag override is not string coercible");
            }

            for (var { tag } of [...explicitTagExtensibleBuiltIns, ...explicitTagTypedArrayBuiltIns, ...internalSlotBuiltIns]) {
                testBuiltIn(tag);
            }
       }
    },
    {
        name: "@@toStringTag lookup does not use HasProperty",
        body: function() {
             var proxy = new Proxy({ [Symbol.toStringTag] : "Proxied" }, {
                // Previous versions of the spec called HasProperty
                has: function (target, key) {
                    assert.fail("Object.prototype.toString should not call HasProperty");
                }
            });
            assert.areEqual("[object Proxied]", Object.prototype.toString.call(proxy), "Proxied @@toStringTag passes through value without calling HasProperty");
        }
    },
    {
        name: "@@toStringTag properties with side effects",
        body: function () {
            var tagThrows = { get [Symbol.toStringTag]() { throw new Error(); } };
            assert.throws(function () { Object.prototype.toString.call(tagThrows) }, Error, "@@toStringTag throws as a side effect");
        }
    },
    {
        name: "VSO OS Bug 1160433: embedded null characters in toStringTag property value",
        body: function () {
            var o = {
                [Symbol.toStringTag]: "before\0after"
            };

            assert.areEqual("[object before\0after]", o.toString(), "ToString implementation handles embedded null characters in @@toStringTag property value");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
