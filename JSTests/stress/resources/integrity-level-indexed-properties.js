const shared = [{ id: 0 }, { id: 1 }, { id: 2 }, { id: 3 }];

function pushed(...values) {
    const array = [];
    for (const value of values)
        array.push(value);
    return array;
}

class Derived extends Array { }

class Species extends Array {
    static result;
    static get [Symbol.species]() { return function () { return Species.result; }; }
}

class StampField extends (function (object) { return object; }) {
    #field = 1;
    static has(object) { return #field in object; }
}

class StampMethod extends (function (object) { return object; }) {
    #method() { }
    static has(object) { return #method in object; }
}

const makers = [
    () => [],
    () => [1, 2, 3],
    () => [1.5, 2.5, 3.5],
    () => ["a", "b", "c"],
    () => pushed(1, 2, 3),
    () => pushed(1.5, 2.5, 3.5),
    () => pushed(shared[0], shared[1], shared[2]),
    () => pushed(1, 2.5, shared[0], "a", undefined, null),
    () => [1, , 3],
    () => { const array = pushed(shared[0], shared[1], shared[2]); delete array[1]; return array; },
    () => new Array(4),
    () => { const array = [1, 2, 3]; array.length = 6; return array; },
    () => { const array = [1, 2, 3]; array.foo = shared[3]; return array; },
    () => { const array = [1, 2, 3]; Object.defineProperty(array, 1, { get() { return 42; }, set(value) { }, enumerable: true, configurable: true }); return array; },
    () => { const array = [1, 2, 3]; Object.defineProperty(array, 1, { value: 2, writable: false, enumerable: false, configurable: true }); return array; },
    () => { const array = [1, 2, 3]; Object.defineProperty(array, "foo", { get() { return 42; }, configurable: true }); return array; },
    () => { const array = [1, 2, 3]; Object.defineProperty(array, "length", { writable: false }); return array; },
    () => Object.preventExtensions([1, 2, 3]),
    () => Object.seal([1, 2, 3]),
    () => { const array = [1, 2, 3]; $vm.ensureArrayStorage(array); array.unshift(0); array.shift(); return array; },
    () => Derived.from([1, 2, 3]),
    () => ({ 0: 1, 1: 2, 2: 3 }),
    () => ({ 0: shared[0], 2: shared[1], foo: 1, length: 3 }),
];

const partiallyFrozenMakers = [
    () => Object.preventExtensions([]),
    () => { const array = [1, 2]; Object.defineProperty(array, 0, { writable: false, configurable: false }); Object.defineProperty(array, 1, { writable: false, configurable: false }); return array; },
    () => { const array = [1, 2]; Object.defineProperty(array, 0, { configurable: false }); Object.defineProperty(array, 1, { configurable: false }); return Object.preventExtensions(array); },
    () => { const array = [1, 2]; Object.defineProperty(array, 0, { writable: false, configurable: false }); Object.defineProperty(array, 1, { writable: false, configurable: false }); return Object.preventExtensions(array); },
    () => { const array = [1, 2]; Object.defineProperty(array, 0, { writable: false, configurable: false }); Object.defineProperty(array, 1, { writable: false }); Object.defineProperty(array, "length", { writable: false }); return Object.preventExtensions(array); },
];

const sparseMakers = [
    () => { const array = [1, 2, 3]; array[100000] = 4; return array; },
    () => { const array = [1, 2, 3]; array[100000] = 4; array.foo = 1; delete array[1]; return array; },
];

const syntaxOperations = [
    "a[0] = v",
    "a[1] = v",
    "a[2] = a[2]",
    "a[a.length] = v",
    "a[a.length + 10] = v",
    "a[4294967294] = v",
    "a[4294967295] = v",
    "a[-1] = v",
    "a.foo = v",
    "a.bar = v",
    "a.length = 0",
    "a.length = a.length",
    "a.length = a.length + 1",
    "a.length = -1",
    "return delete a[0]",
    "return delete a[1]",
    "return delete a[a.length]",
    "return delete a.length",
    "return delete a.foo",
    "a.__proto__ = null",
    "const o = Object.create(a); o[0] = v; return Reflect.ownKeys(o)",
    "const o = Object.create(a); o[1] = v; return Reflect.ownKeys(o)",
    "const o = Object.create(a); o[a.length] = v; return Reflect.ownKeys(o)",
    "const o = Object.create(a); o.length = 0; return Reflect.ownKeys(o)",
    "const o = Object.create(a); o.foo = v; return Reflect.ownKeys(o)",
    "const o = [1, 2, 3]; Object.setPrototypeOf(o, a); delete o[0]; o[0] = v; return [Reflect.ownKeys(o), o[0]]",
];

const callOperations = [
    "return Array.prototype.push.call(a, v)",
    "return Array.prototype.push.call(a)",
    "return Array.prototype.pop.call(a)",
    "return Array.prototype.shift.call(a)",
    "return Array.prototype.unshift.call(a, v)",
    "return Array.prototype.unshift.call(a)",
    "return Array.prototype.splice.call(a, 0, 1)",
    "return Array.prototype.splice.call(a, 0, 0)",
    "return Array.prototype.splice.call(a, 0, 0, v)",
    "return Array.prototype.splice.call(a, 0, 1, v)",
    "return Array.prototype.reverse.call(a)",
    "return Array.prototype.sort.call(a)",
    "return Array.prototype.sort.call(a, () => 0)",
    "return Array.prototype.fill.call(a, v)",
    "return Array.prototype.fill.call(a, v, 0, 0)",
    "return Array.prototype.copyWithin.call(a, 0, 1)",
    "return Object.defineProperty(a, 0, { value: v })",
    "return Object.defineProperty(a, 0, { value: a[0] })",
    "return Object.defineProperty(a, 0, { })",
    "return Object.defineProperty(a, 0, { writable: true })",
    "return Object.defineProperty(a, 0, { writable: false })",
    "return Object.defineProperty(a, 0, { configurable: true })",
    "return Object.defineProperty(a, 0, { enumerable: false })",
    "return Object.defineProperty(a, 0, { get() { } })",
    "return Object.defineProperty(a, 1, { value: v })",
    "return Object.defineProperty(a, 1, { get() { } })",
    "return Object.defineProperty(a, a.length, { value: v })",
    "return Object.defineProperty(a, a.length, { })",
    "return Object.defineProperty(a, 'length', { value: 0 })",
    "return Object.defineProperty(a, 'length', { value: a.length })",
    "return Object.defineProperty(a, 'length', { value: a.length + 1 })",
    "return Object.defineProperty(a, 'length', { writable: true })",
    "return Object.defineProperty(a, 'length', { writable: false })",
    "return Object.defineProperty(a, 'foo', { value: v })",
    "return Object.defineProperty(a, 'bar', { value: v })",
    "return Reflect.set(a, 0, v)",
    "return Reflect.set(a, a.length, v)",
    "return Reflect.set({ }, 0, v, a)",
    "return Reflect.set({ }, a.length, v, a)",
    "return Reflect.set({ }, 'length', 0, a)",
    "return Reflect.set({ }, 'bar', 0, a)",
    "const o = { }; return [Reflect.set(a, 0, v, o), o]",
    "const o = { }; return [Reflect.set(a, a.length, v, o), o]",
    "const o = { }; return [Reflect.set(a, 'length', 0, o), o]",
    "return Reflect.defineProperty(a, 0, { value: v })",
    "return Reflect.defineProperty(a, a.length, { value: v })",
    "return Reflect.deleteProperty(a, 0)",
    "return Reflect.setPrototypeOf(a, null)",
    "return Reflect.setPrototypeOf(a, Reflect.getPrototypeOf(a))",
    "return Object.setPrototypeOf(a, Object.prototype)",
    "return Object.freeze(a)",
    "return Object.seal(a)",
    "return Object.preventExtensions(a)",
    "return Array.prototype.slice.call(a)",
    "const o = Array.prototype.slice.call(a); o[0] = v; o.push(v); return o",
    "return Array.prototype.concat.call(a, [v])",
    "const o = Array.prototype.concat.call(a, []); o[0] = v; return o",
    "return [v].concat(a)",
    "return Array.isArray(a) ? [...a] : null",
    "return Array.prototype.map.call(a, x => x)",
    "return Array.prototype.filter.call(a, x => true)",
    "return Array.prototype.includes.call(a, a[1])",
    "return Array.prototype.includes.call(a, undefined)",
    "return Array.prototype.indexOf.call(a, a[2])",
    "return Array.prototype.join.call(a)",
    "return Array.prototype.with.call(a, 0, v)",
    "return Array.prototype.toSorted.call(a)",
    "return Array.from(a)",
    "return Object.keys(a)",
    "return Object.entries(a)",
    "return Object.assign({ }, a)",
    "return { ...a }",
    "return JSON.stringify(a)",
    "const keys = []; for (const key in a) keys.push(key); return keys",
    "return [0 in a, 1 in a, a.length in a, 'length' in a, Object.hasOwn(a, 1)]",
    "return [a[0], a[1], a[2], a[3], a[a.length], a.length]",
    "new StampField(a); a[0] = v; return StampField.has(a)",
    "new StampMethod(a); a[0] = v; return StampMethod.has(a)",
    "Species.result = a; return Species.from([v, v]).map(x => x)",
    "Species.result = a; return Species.from([v, v]).filter(x => true)",
    "Species.result = a; return Species.from([v, v]).slice(0)",
    "return Array.from.call(function () { return a; }, [v])",
    "return Array.of.call(function () { return a; }, v)",
    "return JSON.parse('[1,[2,3]]', function (key, value) { if (key === '0' && this !== a) this[1] = a; return value === 2 ? v : value; })",
];

function show(value, self) {
    if (typeof value === "function")
        return "function";
    if (typeof value !== "object" || !value)
        return typeof value + ":" + String(value);
    if (value === self)
        return "self";
    return (Array.isArray(value) ? "array" : "object") + "{" + state(value, self) + "}";
}

function state(object, self) {
    let result = "";
    let sealed = !Object.isExtensible(object);
    let frozen = sealed;
    for (const key of Reflect.ownKeys(object)) {
        const descriptor = Reflect.getOwnPropertyDescriptor(object, key);
        if (descriptor.configurable)
            sealed = frozen = false;
        if (descriptor.writable)
            frozen = false;
        result += String(key) + "=";
        result += "value" in descriptor ? show(descriptor.value, self) + (descriptor.writable ? "W" : "-") : "accessor";
        result += (descriptor.enumerable ? "E" : "-") + (descriptor.configurable ? "C" : "-") + ",";
    }
    result += Object.isExtensible(object) ? "X" : "-";
    result += sealed ? "S" : "-";
    result += frozen ? "F" : "-";
    if (Object.isSealed(object) !== sealed || Object.isFrozen(object) !== frozen)
        throw new Error(`isSealed ${Object.isSealed(object)} and isFrozen ${Object.isFrozen(object)} for ${result}`);
    const prototype = Reflect.getPrototypeOf(object);
    result += prototype === Array.prototype ? "A" : prototype === Object.prototype ? "O" : prototype === Derived.prototype ? "D" : prototype === null ? "N" : "?";
    return result;
}

function definePerProperty(object, frozen) {
    Reflect.preventExtensions(object);
    for (const key of Reflect.ownKeys(object)) {
        const descriptor = Reflect.getOwnPropertyDescriptor(object, key);
        Object.defineProperty(object, key, frozen && "value" in descriptor ? { writable: false, configurable: false } : { configurable: false });
    }
    return object;
}

function run(operation, object) {
    let result;
    try {
        result = "returned " + show(operation(object, shared[3]), object);
    } catch (error) {
        result = "threw " + String(error);
    }
    return result + " / " + state(object, object);
}

function compile(source) {
    const operation = new Function("a", "v", source);
    noDFG(operation);
    return [source, operation];
}

const compiledSyntaxOperations = syntaxOperations.flatMap(source => [compile(source), compile("'use strict';" + source)]);
const compiledOperations = compiledSyntaxOperations.concat(callOperations.map(compile));

function test(make, compiled, frozen) {
    const setIntegrityLevel = frozen ? Object.freeze : Object.seal;
    const indexingMode = $vm.indexingMode(make());
    state(make());
    const expectedState = state(definePerProperty(make(), frozen));
    const actualState = state(setIntegrityLevel(make()));
    if (actualState !== expectedState)
        throw new Error(`${make}: expected ${expectedState} but got ${actualState}`);
    if (!actualState.includes(frozen ? "F" : "S", actualState.lastIndexOf(",")))
        throw new Error(`${make}: bad state ${actualState}`);
    for (const [source, operation] of compiled) {
        const expected = run(operation, definePerProperty(make(), frozen));
        const actual = run(operation, setIntegrityLevel(make()));
        if (actual !== expected)
            throw new Error(`${make} ${source}: expected ${expected} but got ${actual}`);
    }
    if ($vm.indexingMode(make()) !== indexingMode)
        throw new Error(`${make}: expected ${indexingMode} but got ${$vm.indexingMode(make())}`);
}

function testExistingPrototype(frozen) {
    for (const source of ["[1, 2, 3]", "({ 0: 1, 1: 2, 2: 3 })"]) {
        const realm = createGlobalObject();
        const prototype = realm.eval(source);
        const child = Object.setPrototypeOf(realm.eval("[, , 3]"), prototype);
        (frozen ? Object.freeze : Object.seal)(prototype);
        child[0] = 42;
        if (Object.hasOwn(child, 0) === frozen)
            throw new Error(`${source}: bad own property 0 of the child`);
    }
}

function testIntegrityLevel(frozen) {
    for (const make of makers)
        test(make, compiledOperations, frozen);
    for (const make of sparseMakers)
        test(make, compiledSyntaxOperations, frozen);
    for (const make of partiallyFrozenMakers)
        test(make, [], frozen);
    testExistingPrototype(frozen);
}
