import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

// Tests that WebAssemblyGCStructure does not do transitions.
// After setting mayBePrototype(true) and replacing the transition path
// with RELEASE_ASSERT_NOT_REACHED, these tests verify that all operations
// that would normally trigger structure transitions on JS objects properly
// reject on Wasm GC objects, and that using Wasm GC objects as prototypes
// of regular objects works without triggering transitions (no crash).

function makeStruct() {
    return instantiate(`
        (module
            (type (struct (field i32) (field f64)))
            (func (export "make") (result (ref 0))
                (struct.new 0 (i32.const 42) (f64.const 3.14)))
            (func (export "getI32") (param (ref 0)) (result i32)
                (struct.get 0 0 (local.get 0)))
            (func (export "getF64") (param (ref 0)) (result f64)
                (struct.get 0 1 (local.get 0)))
        )
    `);
}

function makeArray() {
    return instantiate(`
        (module
            (type (array i32))
            (func (export "make") (result (ref 0))
                (array.new 0 (i32.const 99) (i32.const 5)))
            (func (export "get") (param (ref 0) i32) (result i32)
                (array.get 0 (local.get 0) (local.get 1)))
            (func (export "len") (param (ref 0)) (result i32)
                (array.len (local.get 0)))
        )
    `);
}

function verifyStructIntact(m, obj) {
    assert.eq(m.exports.getI32(obj), 42);
    assert.eq(m.exports.getF64(obj), 3.14);
}

function verifyArrayIntact(m, obj) {
    assert.eq(m.exports.len(obj), 5);
    for (let i = 0; i < 5; i++)
        assert.eq(m.exports.get(obj, i), 99);
}

// =============================================
// PropertyAddition transition patterns
// =============================================

function testPropertyAdditionNamed() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(() => { obj.foo = 42; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj.bar = "hello"; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj["baz"] = true; }, TypeError, "Cannot set property for WebAssembly GC object");

    verifyStructIntact(m, obj);
}

function testPropertyAdditionIndexed() {
    const m = makeStruct();
    const obj = m.exports.make();

    // Indexed properties would trigger indexing type transitions on regular objects
    assert.throws(() => { obj[0] = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[1] = 2; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[100] = 3; }, TypeError, "Cannot set property for WebAssembly GC object");
    // Large index that would trigger ArrayStorage on regular objects
    assert.throws(() => { obj[0xFFFF] = 4; }, TypeError, "Cannot set property for WebAssembly GC object");

    verifyStructIntact(m, obj);
}

function testPropertyAdditionSymbol() {
    const m = makeStruct();
    const obj = m.exports.make();

    const sym = Symbol("test");
    assert.throws(() => { obj[sym] = 42; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[Symbol.iterator] = () => {}; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[Symbol.toPrimitive] = () => 0; }, TypeError, "Cannot set property for WebAssembly GC object");

    verifyStructIntact(m, obj);
}

function testPropertyAdditionOnArray() {
    const m = makeArray();
    const obj = m.exports.make();

    assert.throws(() => { obj.foo = 42; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[0] = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[Symbol("x")] = 1; }, TypeError, "Cannot set property for WebAssembly GC object");

    verifyArrayIntact(m, obj);
}

// =============================================
// PropertyDeletion transition patterns
// =============================================

function testPropertyDeletion() {
    const m = makeStruct();
    const obj = m.exports.make();

    // Delete always throws on Wasm GC objects, even in non-strict mode
    assert.throws(() => { delete obj.foo; }, TypeError, "Cannot delete property for WebAssembly GC object");
    assert.throws(() => { delete obj[0]; }, TypeError, "Cannot delete property for WebAssembly GC object");
    assert.throws(() => { delete obj[Symbol.toPrimitive]; }, TypeError, "Cannot delete property for WebAssembly GC object");

    verifyStructIntact(m, obj);
}

function testPropertyDeletionStrict() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(() => { "use strict"; delete obj.foo; }, TypeError, "Cannot delete property for WebAssembly GC object");
    assert.throws(() => { "use strict"; delete obj[0]; }, TypeError, "Cannot delete property for WebAssembly GC object");

    verifyStructIntact(m, obj);
}

// =============================================
// PropertyAttributeChange / defineProperty transition patterns
// =============================================

function testDefinePropertyData() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(
        () => Object.defineProperty(obj, "foo", { value: 42 }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );
    assert.throws(
        () => Object.defineProperty(obj, "foo", { value: 42, writable: true, enumerable: true, configurable: true }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );

    verifyStructIntact(m, obj);
}

function testDefinePropertyAccessor() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(
        () => Object.defineProperty(obj, "foo", { get: () => 1 }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );
    assert.throws(
        () => Object.defineProperty(obj, "bar", { get: () => 1, set: (v) => {} }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );

    verifyStructIntact(m, obj);
}

function testDefinePropertyIndexed() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(
        () => Object.defineProperty(obj, 0, { value: 42 }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );
    assert.throws(
        () => Object.defineProperty(obj, 100, { value: 42 }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );

    verifyStructIntact(m, obj);
}

function testDefinePropertySymbol() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(
        () => Object.defineProperty(obj, Symbol.iterator, { value: () => {} }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );

    verifyStructIntact(m, obj);
}

function testDefineProperties() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.throws(
        () => Object.defineProperties(obj, { a: { value: 1 }, b: { value: 2 } }),
        TypeError, "Cannot define property for WebAssembly GC object"
    );

    verifyStructIntact(m, obj);
}

// =============================================
// PreventExtensions / Seal / Freeze transition patterns
// =============================================

function testPreventExtensions() {
    const ms = makeStruct();
    const s = ms.exports.make();

    assert.throws(() => Object.preventExtensions(s), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    verifyStructIntact(ms, s);

    const ma = makeArray();
    const a = ma.exports.make();
    assert.throws(() => Object.preventExtensions(a), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    verifyArrayIntact(ma, a);
}

function testSeal() {
    const ms = makeStruct();
    const s = ms.exports.make();

    assert.throws(() => Object.seal(s), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    verifyStructIntact(ms, s);

    const ma = makeArray();
    const a = ma.exports.make();
    assert.throws(() => Object.seal(a), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    verifyArrayIntact(ma, a);
}

function testFreeze() {
    const ms = makeStruct();
    const s = ms.exports.make();

    assert.throws(() => Object.freeze(s), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    verifyStructIntact(ms, s);

    const ma = makeArray();
    const a = ma.exports.make();
    assert.throws(() => Object.freeze(a), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    verifyArrayIntact(ma, a);
}

// =============================================
// ChangePrototype / setPrototypeOf transition patterns
// =============================================

function testSetPrototypeOfWasmObject() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.eq(Object.getPrototypeOf(obj), null);
    assert.throws(() => Object.setPrototypeOf(obj, {}), TypeError, "Cannot set prototype of WebAssembly GC object");
    assert.throws(() => Object.setPrototypeOf(obj, null), TypeError, "Cannot set prototype of WebAssembly GC object");
    assert.throws(() => Object.setPrototypeOf(obj, Object.prototype), TypeError, "Cannot set prototype of WebAssembly GC object");
    assert.eq(Object.getPrototypeOf(obj), null);

    verifyStructIntact(m, obj);
}

function testSetPrototypeOfWasmArray() {
    const m = makeArray();
    const obj = m.exports.make();

    assert.eq(Object.getPrototypeOf(obj), null);
    assert.throws(() => Object.setPrototypeOf(obj, {}), TypeError, "Cannot set prototype of WebAssembly GC object");
    assert.eq(Object.getPrototypeOf(obj), null);

    verifyArrayIntact(m, obj);
}

// =============================================
// BecomePrototype - using Wasm GC object AS a prototype
// This is the key test for setMayBePrototype(true).
// =============================================

function testBecomePrototypeStruct() {
    const m = makeStruct();
    const wasmObj = m.exports.make();

    // Setting a Wasm GC struct as prototype should not crash
    const jsObj = {};
    Object.setPrototypeOf(jsObj, wasmObj);
    assert.eq(Object.getPrototypeOf(jsObj) === wasmObj, true);

    // Property lookups through Wasm GC prototype return undefined
    assert.eq(jsObj.foo, undefined);
    assert.eq(jsObj[0], undefined);
    assert.eq("foo" in jsObj, false);
    assert.eq(Object.hasOwn(jsObj, "foo"), false);

    // defineProperty on jsObj works (bypasses prototype put path)
    Object.defineProperty(jsObj, "x", { value: 10, writable: true, enumerable: true, configurable: true });
    assert.eq(jsObj.x, 10);

    // jsObj's prototype chain ends at wasmObj (null prototype)
    assert.eq(jsObj instanceof Object, false);

    verifyStructIntact(m, wasmObj);
}

function testBecomePrototypeArray() {
    const m = makeArray();
    const wasmArr = m.exports.make();

    const jsObj = {};
    Object.setPrototypeOf(jsObj, wasmArr);
    assert.eq(Object.getPrototypeOf(jsObj) === wasmArr, true);

    verifyArrayIntact(m, wasmArr);
}

function testObjectCreateWithWasmPrototype() {
    const m = makeStruct();
    const wasmObj = m.exports.make();

    const jsObj = Object.create(wasmObj);
    assert.eq(Object.getPrototypeOf(jsObj) === wasmObj, true);

    // Property lookup returns undefined
    assert.eq(jsObj.foo, undefined);

    // for-in yields nothing from prototype chain
    const keys = [];
    for (const k in jsObj)
        keys.push(k);
    assert.eq(keys.length, 0);

    verifyStructIntact(m, wasmObj);
}

function testObjectCreateWithWasmPrototypeAndProperties() {
    const m = makeStruct();
    const wasmObj = m.exports.make();

    // Object.create with property descriptors
    const jsObj = Object.create(wasmObj, {
        myProp: { value: "hello", writable: true, enumerable: true, configurable: true }
    });
    assert.eq(jsObj.myProp, "hello");
    assert.eq(Object.getPrototypeOf(jsObj) === wasmObj, true);

    const keys = [];
    for (const k in jsObj)
        keys.push(k);
    assert.eq(keys.length, 1);
    assert.eq(keys[0], "myProp");

    verifyStructIntact(m, wasmObj);
}

// =============================================
// Repeated BecomePrototype (stress test for no-transition path)
// =============================================

function testRepeatedBecomePrototype() {
    const m = makeStruct();
    const wasmObj = m.exports.make();

    // Use the same Wasm GC object as prototype of many JS objects.
    // If mayBePrototype were not set, each of these could trigger a
    // BecomePrototype transition.
    for (let i = 0; i < 100; i++) {
        const jsObj = Object.create(wasmObj);
        assert.eq(Object.getPrototypeOf(jsObj) === wasmObj, true);
    }

    verifyStructIntact(m, wasmObj);
}

// =============================================
// Multiple objects sharing the same structure
// =============================================

function testMultipleObjectsSameStructure() {
    const m = makeStruct();
    const obj1 = m.exports.make();
    const obj2 = m.exports.make();
    const obj3 = m.exports.make();

    // Attempt transitions on obj1
    assert.throws(() => { obj1.foo = 42; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => Object.seal(obj1), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    assert.throws(() => Object.setPrototypeOf(obj1, {}), TypeError, "Cannot set prototype of WebAssembly GC object");

    // Use obj1 as prototype
    Object.setPrototypeOf({}, obj1);

    // All objects should still work
    verifyStructIntact(m, obj1);
    verifyStructIntact(m, obj2);
    verifyStructIntact(m, obj3);

    // Use obj2 and obj3 as prototypes too
    Object.setPrototypeOf({}, obj2);
    Object.setPrototypeOf({}, obj3);

    verifyStructIntact(m, obj1);
    verifyStructIntact(m, obj2);
    verifyStructIntact(m, obj3);
}

// =============================================
// Objects from different modules with same shape
// =============================================

function testDifferentModulesSameShape() {
    const m1 = makeStruct();
    const m2 = makeStruct();

    const obj1 = m1.exports.make();
    const obj2 = m2.exports.make();

    assert.throws(() => { obj1.x = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj2.x = 1; }, TypeError, "Cannot set property for WebAssembly GC object");

    Object.setPrototypeOf({}, obj1);
    Object.setPrototypeOf({}, obj2);

    verifyStructIntact(m1, obj1);
    verifyStructIntact(m2, obj2);
}

// =============================================
// Casting still works after transition attempts
// =============================================

function testCastAfterTransitionAttempts() {
    const m = instantiate(`
        (module
            (type (sub (struct (field i32))))
            (type (sub 0 (struct (field i32 i64))))
            (func (export "makeBase") (result (ref 0))
                (struct.new 0 (i32.const 10)))
            (func (export "makeDerived") (result (ref 1))
                (struct.new 1 (i32.const 20) (i64.const 30)))
            (func (export "castToBase") (param anyref) (result i32)
                (struct.get 0 0 (ref.cast (ref 0) (local.get 0))))
            (func (export "testBase") (param anyref) (result i32)
                (ref.test (ref 0) (local.get 0)))
            (func (export "testDerived") (param anyref) (result i32)
                (ref.test (ref 1) (local.get 0)))
        )
    `);

    const base = m.exports.makeBase();
    const derived = m.exports.makeDerived();

    // Attempt various transition-triggering operations
    assert.throws(() => { base.x = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => Object.freeze(base), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    assert.throws(() => { derived.x = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => Object.seal(derived), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");

    // Use as prototypes
    Object.setPrototypeOf({}, base);
    Object.setPrototypeOf({}, derived);

    // Casts should still work correctly
    assert.eq(m.exports.castToBase(base), 10);
    assert.eq(m.exports.castToBase(derived), 20);
    assert.eq(m.exports.testBase(base), 1);
    assert.eq(m.exports.testBase(derived), 1);
    assert.eq(m.exports.testDerived(base), 0);
    assert.eq(m.exports.testDerived(derived), 1);
}

function testCastAfterBecomePrototype() {
    const m = instantiate(`
        (module
            (type (sub (struct (field i32))))
            (type (sub 0 (struct (field i32 i64))))
            (func (export "makeBase") (result (ref 0))
                (struct.new 0 (i32.const 10)))
            (func (export "makeDerived") (result (ref 1))
                (struct.new 1 (i32.const 20) (i64.const 30)))
            (func (export "testBase") (param anyref) (result i32)
                (ref.test (ref 0) (local.get 0)))
            (func (export "testDerived") (param anyref) (result i32)
                (ref.test (ref 1) (local.get 0)))
        )
    `);

    const base = m.exports.makeBase();
    const derived = m.exports.makeDerived();

    // Use both as prototypes
    const child1 = Object.create(base);
    const child2 = Object.create(derived);

    // Cast checks should still pass
    assert.eq(m.exports.testBase(base), 1);
    assert.eq(m.exports.testBase(derived), 1);
    assert.eq(m.exports.testDerived(base), 0);
    assert.eq(m.exports.testDerived(derived), 1);

    // Cast checks on child objects should fail (they are regular JS objects)
    assert.eq(m.exports.testBase(child1), 0);
    assert.eq(m.exports.testDerived(child2), 0);
}

// =============================================
// Reflect operations
// =============================================

function testReflectOperations() {
    const m = makeStruct();
    const obj = m.exports.make();

    // Reflect.set throws on Wasm GC objects
    assert.throws(() => Reflect.set(obj, "foo", 42), TypeError, "Cannot set property for WebAssembly GC object");
    // Reflect.deleteProperty throws
    assert.throws(() => Reflect.deleteProperty(obj, "foo"), TypeError, "Cannot delete property for WebAssembly GC object");
    // Reflect.defineProperty returns false
    assert.eq(Reflect.defineProperty(obj, "foo", { value: 42 }), false);
    // Reflect.setPrototypeOf returns false
    assert.eq(Reflect.setPrototypeOf(obj, {}), false);
    assert.eq(Reflect.isExtensible(obj), false);
    // Reflect.preventExtensions throws
    assert.throws(() => Reflect.preventExtensions(obj), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");

    verifyStructIntact(m, obj);
}

// =============================================
// Large struct: all transition types
// =============================================

function testLargeStruct() {
    const m = instantiate(`
        (module
            (type (struct
                (field i32) (field i32) (field i32) (field i32)
                (field i64) (field f32) (field f64)
                (field i32) (field i32) (field i32)))
            (func (export "make") (result (ref 0))
                (struct.new 0
                    (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4)
                    (i64.const 5) (f32.const 6.0) (f64.const 7.0)
                    (i32.const 8) (i32.const 9) (i32.const 10)))
            (func (export "getFirst") (param (ref 0)) (result i32)
                (struct.get 0 0 (local.get 0)))
            (func (export "getLast") (param (ref 0)) (result i32)
                (struct.get 0 9 (local.get 0)))
        )
    `);

    const obj = m.exports.make();

    assert.throws(() => { obj.x = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => { obj[0] = 1; }, TypeError, "Cannot set property for WebAssembly GC object");
    assert.throws(() => Object.defineProperty(obj, "y", { value: 2 }), TypeError, "Cannot define property for WebAssembly GC object");
    assert.throws(() => Object.preventExtensions(obj), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    assert.throws(() => Object.seal(obj), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    assert.throws(() => Object.freeze(obj), TypeError, "Cannot run preventExtensions operation on WebAssembly GC object");
    assert.throws(() => Object.setPrototypeOf(obj, {}), TypeError, "Cannot set prototype of WebAssembly GC object");
    assert.throws(() => { delete obj.x; }, TypeError, "Cannot delete property for WebAssembly GC object");

    Object.setPrototypeOf({}, obj);

    assert.eq(m.exports.getFirst(obj), 1);
    assert.eq(m.exports.getLast(obj), 10);
}

// =============================================
// Assign and spread operators
// =============================================

function testAssignAndSpread() {
    const m = makeStruct();
    const obj = m.exports.make();

    // Object.assign TO a wasm object should throw
    assert.throws(() => Object.assign(obj, { a: 1 }), TypeError, "Cannot set property for WebAssembly GC object");

    // Object.assign FROM a wasm object (as source) should work (no own enumerable props)
    const target = { x: 1 };
    Object.assign(target, obj);
    assert.eq(target.x, 1);
    assert.eq(Object.keys(target).length, 1);

    // Spread should yield empty object
    const spread = { ...obj };
    assert.eq(Object.keys(spread).length, 0);

    verifyStructIntact(m, obj);
}

// =============================================
// JSON.stringify
// =============================================

function testJSONStringify() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.eq(JSON.stringify(obj), "{}");

    verifyStructIntact(m, obj);
}

// =============================================
// Prototype replacement on child
// =============================================

function testPrototypeReplacement() {
    const m = makeStruct();
    const wasmObj = m.exports.make();

    const jsObj = {};
    Object.setPrototypeOf(jsObj, wasmObj);
    assert.eq(Object.getPrototypeOf(jsObj) === wasmObj, true);

    // Change jsObj's prototype away from wasmObj
    Object.setPrototypeOf(jsObj, Object.prototype);
    assert.eq(Object.getPrototypeOf(jsObj) === Object.prototype, true);

    // Change back to wasmObj
    Object.setPrototypeOf(jsObj, wasmObj);
    assert.eq(Object.getPrototypeOf(jsObj) === wasmObj, true);

    verifyStructIntact(m, wasmObj);
}

// =============================================
// Prototype chain: jsObj -> jsObj2 -> wasmObj
// =============================================

function testDeepPrototypeChain() {
    const m = makeStruct();
    const wasmObj = m.exports.make();

    const jsObj2 = Object.create(wasmObj);
    Object.defineProperty(jsObj2, "middle", { value: "middle", enumerable: true, configurable: true, writable: true });
    const jsObj1 = Object.create(jsObj2);

    assert.eq(jsObj1.middle, "middle");
    assert.eq(Object.getPrototypeOf(Object.getPrototypeOf(jsObj1)) === wasmObj, true);

    verifyStructIntact(m, wasmObj);
}

// =============================================
// isExtensible
// =============================================

function testIsExtensible() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.eq(Object.isExtensible(obj), false);
    assert.eq(Reflect.isExtensible(obj), false);

    const ma = makeArray();
    const arr = ma.exports.make();
    assert.eq(Object.isExtensible(arr), false);

    verifyStructIntact(m, obj);
    verifyArrayIntact(ma, arr);
}

// =============================================
// getOwnPropertyDescriptor / getOwnPropertyNames
// =============================================

function testPropertyEnumeration() {
    const m = makeStruct();
    const obj = m.exports.make();

    assert.eq(Object.getOwnPropertyDescriptor(obj, "foo"), undefined);
    assert.eq(Object.getOwnPropertyDescriptor(obj, 0), undefined);
    assert.eq(Object.getOwnPropertyNames(obj).length, 0);
    assert.eq(Object.getOwnPropertySymbols(obj).length, 0);
    assert.eq(Object.keys(obj).length, 0);
    assert.eq(Reflect.ownKeys(obj).length, 0);

    verifyStructIntact(m, obj);
}

// Run all tests
testPropertyAdditionNamed();
testPropertyAdditionIndexed();
testPropertyAdditionSymbol();
testPropertyAdditionOnArray();
testPropertyDeletion();
testPropertyDeletionStrict();
testDefinePropertyData();
testDefinePropertyAccessor();
testDefinePropertyIndexed();
testDefinePropertySymbol();
testDefineProperties();
testPreventExtensions();
testSeal();
testFreeze();
testSetPrototypeOfWasmObject();
testSetPrototypeOfWasmArray();
testBecomePrototypeStruct();
testBecomePrototypeArray();
testObjectCreateWithWasmPrototype();
testObjectCreateWithWasmPrototypeAndProperties();
testRepeatedBecomePrototype();
testMultipleObjectsSameStructure();
testDifferentModulesSameShape();
testCastAfterTransitionAttempts();
testCastAfterBecomePrototype();
testReflectOperations();
testLargeStruct();
testAssignAndSpread();
testJSONStringify();
testPrototypeReplacement();
testDeepPrototypeChain();
testIsExtensible();
testPropertyEnumeration();
