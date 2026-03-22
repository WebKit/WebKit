// Tests that WasmGC objects (which have no JS realm) interact safely with
// JavaScript operations that may internally access realm(). WasmGC structs
// and arrays have realmless Structures, so structure()->realm() returns null.

import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

const m = instantiate(`
  (module
    (type $Struct (struct (field (mut i32))))
    (type $Array (array (mut i32)))
    (func (export "makeStruct") (result (ref $Struct))
      (struct.new $Struct (i32.const 42)))
    (func (export "makeArray") (result (ref $Array))
      (array.new $Array (i32.const 7) (i32.const 3)))
  )
`);

const wasmStruct = m.exports.makeStruct();
const wasmArray = m.exports.makeArray();

// Helper: assert that func throws a TypeError (without matching message text).
function mustThrowTypeError(func, label) {
    try {
        func();
    } catch (e) {
        if (e instanceof TypeError)
            return;
        throw new Error(`${label}: expected TypeError, got ${e.name}: ${e.message}`);
    }
    throw new Error(`${label}: expected TypeError but no exception was thrown`);
}

// --- Tests for FIXED call sites (confirm fixes work, no crash) ---

// typeof should work (no realm needed).
assert.eq(typeof wasmStruct, "object");
assert.eq(typeof wasmArray, "object");

// Calling a non-callable WasmGC object should throw TypeError (exercises
// calculatedClassName which previously could deref null realm).
mustThrowTypeError(() => wasmStruct(), "call wasmStruct");
mustThrowTypeError(() => wasmArray(), "call wasmArray");

// String coercion exercises toPrimitive / calculatedClassName paths.
mustThrowTypeError(() => String(wasmStruct), "String(wasmStruct)");
mustThrowTypeError(() => String(wasmArray), "String(wasmArray)");

// Array.prototype.toString.call — exercises canUseDefaultArrayJoinForToString
// which calls realm() internally.
{
    let result = Array.prototype.toString.call(wasmStruct);
    assert.eq(typeof result, "string");
}

// [].concat(wasmGCObj) — exercises arrayMissingIsConcatSpreadable (A4 fix).
{
    let result = [].concat(wasmStruct);
    assert.eq(result.length, 1);
    assert.eq(result[0], wasmStruct);
}

{
    let result = [1, 2].concat(wasmArray);
    assert.eq(result.length, 3);
    assert.eq(result[0], 1);
    assert.eq(result[1], 2);
    assert.eq(result[2], wasmArray);
}

// --- Tests for SAFE-by-type-guard sites (confirm no crash) ---

// Object.prototype.toString.call — uses classInfo / @@toStringTag, not realm.
{
    let result = Object.prototype.toString.call(wasmStruct);
    assert.eq(typeof result, "string");
}

// JSON.stringify — getOwnPropertySlot returns false for WasmGC objects.
{
    let result = JSON.stringify(wasmStruct);
    // WasmGC objects have no enumerable properties, result could be undefined or "{}".
    assert.truthy(result === undefined || result === "{}" || typeof result === "string");
}

// Object.keys — should return empty array.
{
    let result = Object.keys(wasmStruct);
    assert.isArray(result);
    assert.eq(result.length, 0);
}

// Object.getOwnPropertyNames — should return empty array.
{
    let result = Object.getOwnPropertyNames(wasmStruct);
    assert.isArray(result);
    assert.eq(result.length, 0);
}

// Object.getOwnPropertyDescriptor — should return undefined.
{
    let result = Object.getOwnPropertyDescriptor(wasmStruct, "x");
    assert.eq(result, undefined);
}

// --- Tests for SAFE-by-put-override sites (WasmGC put throws) ---

mustThrowTypeError(() => { wasmStruct.x = 1; }, "wasmStruct.x = 1");
mustThrowTypeError(() => { wasmStruct[0] = 1; }, "wasmStruct[0] = 1");
mustThrowTypeError(() => Object.setPrototypeOf(wasmStruct, {}), "setPrototypeOf(wasmStruct)");

mustThrowTypeError(() => { wasmArray.x = 1; }, "wasmArray.x = 1");
mustThrowTypeError(() => { wasmArray[0] = 1; }, "wasmArray[0] = 1");
mustThrowTypeError(() => Object.setPrototypeOf(wasmArray, {}), "setPrototypeOf(wasmArray)");

// --- Tests for SAFE-by-callable-guard sites ---

assert.eq(typeof wasmStruct, "object"); // Not "function"
mustThrowTypeError(() => wasmStruct(), "call wasmStruct (2)");
mustThrowTypeError(() => new wasmStruct(), "new wasmStruct");

assert.eq(typeof wasmArray, "object");
mustThrowTypeError(() => wasmArray(), "call wasmArray (2)");
mustThrowTypeError(() => new wasmArray(), "new wasmArray");

// --- Tests for SAFE-by-IC/indexing-guard sites ---

// Property access on non-existent property — getOwnPropertySlot returns false.
assert.eq(wasmStruct.nonExistent, undefined);
assert.eq(wasmArray.nonExistent, undefined);

// "in" operator.
assert.eq("x" in wasmStruct, false);
assert.eq(0 in wasmArray, false);

// for-in should produce no iterations.
{
    let count = 0;
    for (let k in wasmStruct) count++;
    assert.eq(count, 0);
}

{
    let count = 0;
    for (let k in wasmArray) count++;
    assert.eq(count, 0);
}

// Object.assign — should produce empty copy since no own enumerable properties.
{
    let result = Object.assign({}, wasmStruct);
    assert.eq(Object.keys(result).length, 0);
}

// --- Tests for SAFE-by-iterable-guard ---

// WasmGC objects are not iterable — these operations fall back to treating
// them as array-like with length 0, producing empty results (no crash).
{
    let ta = new Uint8Array(wasmStruct);
    assert.eq(ta.length, 0);
}

{
    let result = Array.from(wasmStruct);
    assert.isArray(result);
    assert.eq(result.length, 0);
}

// ==========================================================================
// Tests for Structure Transition Safety
//
// WebAssemblyGCStructure must NEVER undergo transitions. The following tests
// exercise every TransitionKind path to verify they are blocked.
// ==========================================================================

// --- TransitionKind::BecomePrototype ---
// Using a Wasm GC object AS a prototype is safe because mayBePrototype is
// pre-set in the WebAssemblyGCStructure constructor.
{
    let child = Object.create(wasmStruct);
    assert.eq(Object.getPrototypeOf(child), wasmStruct);
}
{
    let child = Object.create(wasmArray);
    assert.eq(Object.getPrototypeOf(child), wasmArray);
}
{
    let o = {};
    Object.setPrototypeOf(o, wasmStruct);
    assert.eq(Object.getPrototypeOf(o), wasmStruct);
}
{
    let o = {};
    Object.setPrototypeOf(o, wasmArray);
    assert.eq(Object.getPrototypeOf(o), wasmArray);
}
{
    let o = {};
    Reflect.setPrototypeOf(o, wasmStruct);
    assert.eq(Object.getPrototypeOf(o), wasmStruct);
}
{
    let o = { __proto__: wasmStruct };
    assert.eq(Object.getPrototypeOf(o), wasmStruct);
}

// --- TransitionKind::PropertyAddition ---
mustThrowTypeError(() => { wasmStruct.newProp = 1; }, "struct PropertyAddition via put");
mustThrowTypeError(() => { wasmArray.newProp = 1; }, "array PropertyAddition via put");
mustThrowTypeError(() => Object.defineProperty(wasmStruct, 'foo', { value: 1 }), "struct defineProperty");
mustThrowTypeError(() => Object.defineProperty(wasmArray, 'foo', { value: 1 }), "array defineProperty");
mustThrowTypeError(() => Object.defineProperties(wasmStruct, { a: { value: 1 } }), "struct defineProperties");
mustThrowTypeError(() => Object.defineProperties(wasmArray, { a: { value: 1 } }), "array defineProperties");

// --- TransitionKind::PropertyDeletion ---
mustThrowTypeError(() => { delete wasmStruct.x; }, "struct delete named");
mustThrowTypeError(() => { delete wasmArray.x; }, "array delete named");
mustThrowTypeError(() => { delete wasmStruct[0]; }, "struct delete indexed");
mustThrowTypeError(() => { delete wasmArray[0]; }, "array delete indexed");

// --- TransitionKind::PropertyAttributeChange ---
mustThrowTypeError(() => Object.defineProperty(wasmStruct, 'x', { writable: false }), "struct attr change writable");
mustThrowTypeError(() => Object.defineProperty(wasmStruct, 'x', { get() { return 1; } }), "struct attr change accessor");
mustThrowTypeError(() => Object.defineProperty(wasmArray, 'x', { writable: false }), "array attr change writable");

// --- TransitionKind::PreventExtensions ---
mustThrowTypeError(() => Object.preventExtensions(wasmStruct), "struct preventExtensions");
mustThrowTypeError(() => Object.preventExtensions(wasmArray), "array preventExtensions");
// Reflect.preventExtensions also throws because the method table override throws
// before Reflect can convert it to a return value.
mustThrowTypeError(() => Reflect.preventExtensions(wasmStruct), "struct Reflect.preventExtensions");
mustThrowTypeError(() => Reflect.preventExtensions(wasmArray), "array Reflect.preventExtensions");

// --- TransitionKind::Seal ---
mustThrowTypeError(() => Object.seal(wasmStruct), "struct seal");
mustThrowTypeError(() => Object.seal(wasmArray), "array seal");

// --- TransitionKind::Freeze ---
mustThrowTypeError(() => Object.freeze(wasmStruct), "struct freeze");
mustThrowTypeError(() => Object.freeze(wasmArray), "array freeze");

// --- TransitionKind::ChangePrototype (setting proto OF a Wasm GC object) ---
mustThrowTypeError(() => Object.setPrototypeOf(wasmStruct, {}), "struct setPrototypeOf obj");
mustThrowTypeError(() => Object.setPrototypeOf(wasmStruct, null), "struct setPrototypeOf null");
mustThrowTypeError(() => Object.setPrototypeOf(wasmArray, {}), "array setPrototypeOf obj");
mustThrowTypeError(() => Object.setPrototypeOf(wasmArray, null), "array setPrototypeOf null");
// Reflect.setPrototypeOf returns false (setPrototype override respects shouldThrowIfCantSet).
assert.eq(Reflect.setPrototypeOf(wasmStruct, {}), false);
assert.eq(Reflect.setPrototypeOf(wasmArray, {}), false);
mustThrowTypeError(() => { wasmStruct.__proto__ = {}; }, "struct __proto__ assign");
mustThrowTypeError(() => { wasmArray.__proto__ = {}; }, "array __proto__ assign");

// --- Indexing type transitions (putByIndex) ---
mustThrowTypeError(() => { wasmStruct[0] = 42; }, "struct int32 index");
mustThrowTypeError(() => { wasmStruct[0] = 3.14; }, "struct double index");
mustThrowTypeError(() => { wasmStruct[0] = "hello"; }, "struct string index");
mustThrowTypeError(() => { wasmArray[100] = 1; }, "array out-of-bounds index");
mustThrowTypeError(() => { wasmArray[0] = 3.14; }, "array double index");
mustThrowTypeError(() => { wasmArray[0] = "hello"; }, "array string index");

// --- Object.assign into Wasm GC target ---
mustThrowTypeError(() => Object.assign(wasmStruct, { a: 1 }), "struct Object.assign");
mustThrowTypeError(() => Object.assign(wasmArray, { a: 1 }), "array Object.assign");

// --- Proxy wrapping ---
{
    let proxy = new Proxy(wasmStruct, {});
    mustThrowTypeError(() => { proxy.x = 1; }, "proxy struct set");
    mustThrowTypeError(() => Object.defineProperty(proxy, 'x', { value: 1 }), "proxy struct defineProperty");
    mustThrowTypeError(() => { delete proxy.x; }, "proxy struct delete");
    mustThrowTypeError(() => Object.preventExtensions(proxy), "proxy struct preventExtensions");
    mustThrowTypeError(() => Object.seal(proxy), "proxy struct seal");
    mustThrowTypeError(() => Object.freeze(proxy), "proxy struct freeze");
}
{
    let proxy = new Proxy(wasmArray, {});
    mustThrowTypeError(() => { proxy.x = 1; }, "proxy array set");
    mustThrowTypeError(() => Object.defineProperty(proxy, 'x', { value: 1 }), "proxy array defineProperty");
    mustThrowTypeError(() => { delete proxy.x; }, "proxy array delete");
    mustThrowTypeError(() => Object.preventExtensions(proxy), "proxy array preventExtensions");
    mustThrowTypeError(() => Object.seal(proxy), "proxy array seal");
    mustThrowTypeError(() => Object.freeze(proxy), "proxy array freeze");
}

// --- JIT stress: loop to trigger compilation on transition-blocked paths ---
for (let i = 0; i < 10000; i++) {
    try { wasmStruct.x = 1; } catch(e) {}
    try { Object.defineProperty(wasmStruct, 'y', { value: 1 }); } catch(e) {}
    try { delete wasmStruct.z; } catch(e) {}
    Object.create(wasmStruct);  // didBecomePrototype path — should not crash
}

// --- isExtensible returns false ---
assert.eq(Object.isExtensible(wasmStruct), false);
assert.eq(Object.isExtensible(wasmArray), false);
assert.eq(Reflect.isExtensible(wasmStruct), false);
assert.eq(Reflect.isExtensible(wasmArray), false);

// --- isFrozen / isSealed reflect non-extensible with no own properties ---
assert.eq(Object.isFrozen(wasmStruct), true);
assert.eq(Object.isSealed(wasmStruct), true);
assert.eq(Object.isFrozen(wasmArray), true);
assert.eq(Object.isSealed(wasmArray), true);
