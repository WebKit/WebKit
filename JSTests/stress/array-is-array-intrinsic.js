function assert(cond, msg) {
    if (!cond) throw new Error(msg ?? "assertion failed");
}

// ---- Correctness: spec edge cases ----

// Non-cell primitives
assert(Array.isArray(undefined) === false, "undefined");
assert(Array.isArray(null) === false, "null");
assert(Array.isArray(true) === false, "true");
assert(Array.isArray(false) === false, "false");
assert(Array.isArray(0) === false, "0");
assert(Array.isArray(-0) === false, "-0");
assert(Array.isArray(NaN) === false, "NaN");
assert(Array.isArray(Infinity) === false, "Infinity");
assert(Array.isArray(1.5) === false, "1.5");
assert(Array.isArray(42n) === false, "BigInt");

// Cell non-arrays
assert(Array.isArray("string") === false, "string");
assert(Array.isArray({}) === false, "plain object");
assert(Array.isArray(function () { }) === false, "function");
assert(Array.isArray(/regex/) === false, "regexp");
assert(Array.isArray(new Map()) === false, "Map");
assert(Array.isArray(new Set()) === false, "Set");
assert(Array.isArray(Symbol()) === false, "Symbol");

// Arrays
assert(Array.isArray([]) === true, "empty array");
assert(Array.isArray([1, 2, 3]) === true, "array literal");
assert(Array.isArray(new Array(5)) === true, "new Array");

// Derived arrays (subclasses — DerivedArrayType)
class MyArray extends Array { }
assert(Array.isArray(new MyArray()) === true, "derived array");
assert(Array.isArray(new MyArray(3)) === true, "derived array with size");

// Zero-argument call -> false (spec: arg is undefined)
assert(Array.isArray() === false, "no args");

// Proxy wrapping an array -> true (spec: IsArray recurses through proxy)
const proxiedArray = new Proxy([], {});
assert(Array.isArray(proxiedArray) === true, "Proxy(array)");

// Proxy wrapping a non-array -> false
const proxiedObj = new Proxy({}, {});
assert(Array.isArray(proxiedObj) === false, "Proxy(object)");

// Nested proxy -> true (spec recurses)
const doubleProxied = new Proxy(new Proxy([], {}), {});
assert(Array.isArray(doubleProxied) === true, "Proxy(Proxy(array))");

// Revoked proxy -> throws TypeError
const { proxy: revokedProxy, revoke } = Proxy.revocable([], {});
revoke();
let threw = false;
try { Array.isArray(revokedProxy); } catch (e) { threw = e instanceof TypeError; }
assert(threw, "revoked proxy must throw TypeError");

// ---- Tier tests ----
// Each function is called enough times to force up through the tiers.
// Mixing non-cell and cell inputs in the same hot function was the bug trigger.

// LLInt / Baseline (no DFG)
function testMixedNoDFG(v) {
    return Array.isArray(v);
}
noDFG(testMixedNoDFG);
noFTL(testMixedNoDFG);

for (let i = 0; i < 1000; i++) {
    assert(testMixedNoDFG(undefined) === false);
    assert(testMixedNoDFG([]) === true);
    assert(testMixedNoDFG(null) === false);
    assert(testMixedNoDFG(true) === false);
    assert(testMixedNoDFG({}) === false);
}

// DFG (no FTL): mixed types must not cause BadType exits / jettisons
function testMixedDFG(v) {
    return Array.isArray(v);
}
noFTL(testMixedDFG);

const mixed = [undefined, [], null, false, {}, new MyArray(), 42, "str", new Proxy([], {})];
for (let i = 0; i < 10000; i++) {
    const result = testMixedDFG(mixed[i % mixed.length]);
    assert(typeof result === "boolean", `DFG: expected boolean for input index ${i % mixed.length}`);
}

// DFG: known-array input — abstract interpreter should fold to constant true
function testKnownArray(arr) {
    return Array.isArray(arr);
}
noFTL(testKnownArray);
noInline(testKnownArray);

for (let i = 0; i < 10000; i++) {
    assert(testKnownArray([1, 2, 3]) === true, "DFG constant fold: known array");
}

// DFG: known non-cell — abstract interpreter should fold to constant false
function testKnownNonCell(x) {
    return Array.isArray(x);
}
noFTL(testKnownNonCell);
noInline(testKnownNonCell);

for (let i = 0; i < 10000; i++) {
    assert(testKnownNonCell(i) === false, "DFG constant fold: known number");
}

// FTL: hot mixed-type loop — must compile stably without exits
function testFTLMixed(v) {
    return Array.isArray(v);
}
noInline(testFTLMixed);

const ftlInputs = [[], new MyArray(), null, undefined, false, {}, 0, "s"];
for (let i = 0; i < 200000; i++) {
    testFTLMixed(ftlInputs[i & 7]);
}
assert(testFTLMixed([]) === true, "FTL: array");
assert(testFTLMixed(new MyArray()) === true, "FTL: derived array");
assert(testFTLMixed(null) === false, "FTL: null");
assert(testFTLMixed(undefined) === false, "FTL: undefined");
assert(testFTLMixed({}) === false, "FTL: plain object");

// FTL: known-array constant folding in hot path
function testFTLKnownArray(arr) {
    return Array.isArray(arr);
}
noInline(testFTLKnownArray);

const arrInput = [1];
for (let i = 0; i < 200000; i++) {
    assert(testFTLKnownArray(arrInput) === true, "FTL constant fold: known array");
}

// FTL: proxy slow path reachable at FTL tier
function testFTLProxy(v) {
    return Array.isArray(v);
}
noInline(testFTLProxy);

const proxyArr = new Proxy([], {});
for (let i = 0; i < 200000; i++) {
    // Warm with arrays (fast path), occasionally hit the proxy slow path
    testFTLProxy(i % 100 === 0 ? proxyArr : []);
}
assert(testFTLProxy(proxyArr) === true, "FTL proxy slow path returns true");
assert(testFTLProxy(new Proxy({}, {})) === false, "FTL proxy slow path returns false");
