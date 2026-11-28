//@ runDefault
//@ run("no-jit", "--useJIT=0")
//@ run("baseline", "--useDFGJIT=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10")
//@ run("lol", "--useDFGJIT=0", "--useLOLJIT=1", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10")
//@ run("no-ftl", "--useFTLJIT=0")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": bad value " + String(actual) + ", expected " + String(expected));
}
noInline(shouldBe);

let other = createGlobalObject();

const source = `[
    function isObject(v) { return typeof v === "object"; },
    function isFunction(v) { return typeof v === "function"; },
    function looseIsObject(v) { return typeof v == "object"; },
    function looseIsFunction(v) { return typeof v == "function"; },
    function isNotObject(v) { return typeof v !== "object"; },
    function isNotFunction(v) { return typeof v !== "function"; },
    function branchObject(v) { if (typeof v === "object") return true; return false; },
    function branchFunction(v) { if (typeof v === "function") return true; return false; },
    function overwriteObject(v) { v = typeof v === "object"; return v; },
    function overwriteFunction(v) { v = typeof v === "function"; return v; },
    function keepObject(v) { let r = typeof v === "object"; return [r, v]; },
    function keepFunction(v) { let r = typeof v === "function"; return [r, v]; },
    function liveObject(v, a, b, c) { let x = a + b; let r = typeof v === "object"; return r ? x + c : x - c; },
    function liveFunction(v, a, b, c) { let x = a + b; let r = typeof v === "function"; return r ? x + c : x - c; },
]`;

function functionsFor(realm) {
    let functions = realm.eval(source);
    for (let f of functions)
        noInline(f);
    return functions;
}

let here = functionsFor(globalThis);
let there = functionsFor(other);

function check(functions, v, type, name) {
    let [isObject, isFunction, looseIsObject, looseIsFunction, isNotObject, isNotFunction, branchObject, branchFunction,
        overwriteObject, overwriteFunction, keepObject, keepFunction, liveObject, liveFunction] = functions;
    let object = type === "object";
    let func = type === "function";
    shouldBe(isObject(v), object, name);
    shouldBe(isFunction(v), func, name);
    shouldBe(looseIsObject(v), object, name);
    shouldBe(looseIsFunction(v), func, name);
    shouldBe(isNotObject(v), !object, name);
    shouldBe(isNotFunction(v), !func, name);
    shouldBe(branchObject(v), object, name);
    shouldBe(branchFunction(v), func, name);
    shouldBe(overwriteObject(v), object, name);
    shouldBe(overwriteFunction(v), func, name);

    let kept = keepObject(v);
    shouldBe(kept[0], object, name);
    shouldBe(Object.is(kept[1], v), true, name);
    kept = keepFunction(v);
    shouldBe(kept[0], func, name);
    shouldBe(Object.is(kept[1], v), true, name);

    shouldBe(liveObject(v, 100, 20, 3), object ? 123 : 117, name);
    shouldBe(liveFunction(v, 100, 20, 3), func ? 123 : 117, name);
}
noInline(check);

function* generator() { }
async function asyncFunction() { }
async function* asyncGenerator() { }
class Base { method() { } static #p() { } static get p() { return Base.#p; } }
class Derived extends Base { }
class DerivedArray extends Array { }
class DerivedString extends String { }
class DerivedFunction extends Function { }
function sloppyArguments(a) { return arguments; }
function scopedArguments(a) { (() => a)(); return arguments; }
function strictArguments(a) { "use strict"; return arguments; }

let revoked = Proxy.revocable({}, {});
revoked.revoke();
let revokedFunction = Proxy.revocable(function () { }, {});
revokedFunction.revoke();
let throwingHandler = new Proxy({}, { get() { throw new Error("typeof must not call a trap"); } });

let dictionary = {};
for (let i = 0; i < 200; ++i)
    dictionary["p" + i] = i;
for (let i = 0; i < 100; ++i)
    delete dictionary["p" + i];

let functionWithNewStructure = function () { };
functionWithNewStructure.property = 42;
let functionWithNullPrototype = Object.setPrototypeOf(function () { }, null);
let constructorWithNewStructure = other.Map;
constructorWithNewStructure.property = 42;

const ropeSeed = 2;
const values = [
    ["undefined", undefined, "undefined"],
    ["null", null, "object"],
    ["true", true, "boolean"],
    ["false", false, "boolean"],
    ["0", 0, "number"],
    ["-1", -1, "number"],
    ["int32 max", 0x7fffffff, "number"],
    ["double", 1.5, "number"],
    ["NaN", NaN, "number"],
    ["-0", -0, "number"],
    ["string", "a", "string"],
    ["rope", "abc" + String(12345 * ropeSeed), "string"],
    ["symbol", Symbol("s"), "symbol"],
    ["well-known symbol", Symbol.iterator, "symbol"],
    ["bigint", 1n, "bigint"],
    ["large bigint", 1n << 200n, "bigint"],

    ["plain object", {}, "object"],
    ["object with properties", { a: 1, b: 2 }, "object"],
    ["null prototype", Object.create(null), "object"],
    ["dictionary", dictionary, "object"],
    ["frozen", Object.freeze({ a: 1 }), "object"],
    ["array", [], "object"],
    ["double array", [1.5], "object"],
    ["derived array", new DerivedArray, "object"],
    ["class instance", new Derived, "object"],
    ["regexp", /a/, "object"],
    ["date", new Date(0), "object"],
    ["error", new Error, "object"],
    ["map", new Map, "object"],
    ["set", new Set, "object"],
    ["weak map", new WeakMap, "object"],
    ["weak ref", new WeakRef({}), "object"],
    ["promise", Promise.resolve(), "object"],
    ["array buffer", new ArrayBuffer(8), "object"],
    ["typed array", new Int32Array(1), "object"],
    ["data view", new DataView(new ArrayBuffer(8)), "object"],
    ["Number object", new Number(1), "object"],
    ["Boolean object", new Boolean(true), "object"],
    ["String object", new String("a"), "object"],
    ["derived String object", new DerivedString("a"), "object"],
    ["Symbol object", Object(Symbol()), "object"],
    ["BigInt object", Object(1n), "object"],
    ["generator object", generator(), "object"],
    ["async generator object", asyncGenerator(), "object"],
    ["array iterator", [][Symbol.iterator](), "object"],
    ["sloppy arguments", sloppyArguments(1), "object"],
    ["scoped arguments", scopedArguments(1), "object"],
    ["strict arguments", strictArguments(1), "object"],
    ["Math", Math, "object"],
    ["JSON", JSON, "object"],
    ["Reflect", Reflect, "object"],
    ["Object.prototype", Object.prototype, "object"],
    ["Array.prototype", Array.prototype, "object"],
    ["globalThis", globalThis, "object"],
    ["other realm's global", other, "object"],
    ["other realm's object", other.eval("({})"), "object"],

    ["function", function () { }, "function"],
    ["arrow", () => { }, "function"],
    ["method", Base.prototype.method, "function"],
    ["private method", Base.p, "function"],
    ["generator function", generator, "function"],
    ["async function", asyncFunction, "function"],
    ["async arrow", async () => { }, "function"],
    ["async generator function", asyncGenerator, "function"],
    ["class", Base, "function"],
    ["derived class", Derived, "function"],
    ["function with new structure", functionWithNewStructure, "function"],
    ["function with null prototype", functionWithNullPrototype, "function"],
    ["Function()", Function("return 1"), "function"],
    ["derived Function", new DerivedFunction("return 1"), "function"],
    ["bound", function () { }.bind(null), "function"],
    ["bound native", Math.abs.bind(null), "function"],
    ["native", Math.abs, "function"],
    ["builtin", Array.prototype.map, "function"],
    ["getter", Object.getOwnPropertyDescriptor(Map.prototype, "size").get, "function"],
    ["Function.prototype.caller getter", Object.getOwnPropertyDescriptor(Function.prototype, "caller").get, "function"],
    ["ThrowTypeError", Object.getOwnPropertyDescriptor(strictArguments(1), "callee").get, "function"],
    ["Function.prototype", Function.prototype, "function"],
    ["Object", Object, "function"],
    ["Array", Array, "function"],
    ["Map", Map, "function"],
    ["Proxy", Proxy, "function"],
    ["Symbol", Symbol, "function"],
    ["Int8Array", Int8Array, "function"],
    ["Promise", Promise, "function"],
    ["Promise resolving function", Promise.withResolvers().resolve, "function"],
    ["Proxy.revocable's revoke", Proxy.revocable({}, {}).revoke, "function"],
    ["other realm's function", other.eval("(function () { })"), "function"],
    ["other realm's constructor", other.Array, "function"],
    ["other realm's constructor with new structure", constructorWithNewStructure, "function"],

    ["proxy of object", new Proxy({}, {}), "object"],
    ["proxy of array", new Proxy([], {}), "object"],
    ["proxy of function", new Proxy(function () { }, {}), "function"],
    ["proxy of arrow", new Proxy(() => { }, {}), "function"],
    ["proxy of class", new Proxy(Base, {}), "function"],
    ["proxy of native", new Proxy(Map, {}), "function"],
    ["proxy of proxy of object", new Proxy(new Proxy({}, {}), {}), "object"],
    ["proxy of proxy of function", new Proxy(new Proxy(Base, {}), {}), "function"],
    ["proxy with traps", new Proxy({}, throwingHandler), "object"],
    ["proxy of function with traps", new Proxy(Base, throwingHandler), "function"],
    ["revoked proxy of object", revoked.proxy, "object"],
    ["revoked proxy of function", revokedFunction.proxy, "function"],

    ["masquerader", makeMasquerader(), "undefined", "function"],
    ["other realm's masquerader", other.makeMasquerader(), "function", "undefined"],
    ["proxy of masquerader", new Proxy(makeMasquerader(), {}), "function"],
];

function constantNullIsObject() { return typeof null === "object"; }
function constantNullIsFunction() { return typeof null === "function"; }
function constantUndefinedIsObject() { return typeof (void 0) === "object"; }
function constantStringIsObject() { return typeof "a" === "object"; }
function constantStringIsFunction() { return typeof "a" === "function"; }
function constantBigIntIsObject() { return typeof 100000000000000000000000n === "object"; }
function literalIsObject() { return typeof {} === "object"; }
function literalIsFunction() { return typeof (() => { }) === "function"; }
noInline(constantNullIsObject);
noInline(constantNullIsFunction);
noInline(constantUndefinedIsObject);
noInline(constantStringIsObject);
noInline(constantStringIsFunction);
noInline(constantBigIntIsObject);
noInline(literalIsObject);
noInline(literalIsFunction);

const iterations = Math.max(5, Math.min(20, Math.floor(testLoopCount / 400)));
for (let i = 0; i < iterations; ++i) {
    for (let [name, v, type, typeThere = type] of values) {
        check(here, v, type, name);
        check(there, v, typeThere, name + " (other realm's code)");
    }

    shouldBe(constantNullIsObject(), true, "constant null");
    shouldBe(constantNullIsFunction(), false, "constant null");
    shouldBe(constantUndefinedIsObject(), false, "constant undefined");
    shouldBe(constantStringIsObject(), false, "constant string");
    shouldBe(constantStringIsFunction(), false, "constant string");
    shouldBe(constantBigIntIsObject(), false, "constant bigint");
    shouldBe(literalIsObject(), true, "literal object");
    shouldBe(literalIsFunction(), true, "literal function");
}
