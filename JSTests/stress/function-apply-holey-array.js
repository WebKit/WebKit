function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function describe() {
    let result = arguments.length + ":";
    for (let i = 0; i < arguments.length; ++i)
        result += (i ? "," : "") + String(arguments[i]);
    return result;
}
noInline(describe);

function describeInlined(a, b, c, d) {
    return arguments.length + ":" + a + "," + b + "," + c + "," + d;
}

function describeSparse() {
    return arguments.length + ":" + arguments[0] + "," + arguments[1] + "," + arguments[10000] + "," + arguments[20000];
}
noInline(describeSparse);

function apply(array) { return describe.apply(null, array); }
noInline(apply);

function applyInlined(array) { return describeInlined.apply(null, array); }
noInline(applyInlined);

function applySparse(array) { return describeSparse.apply(null, array); }
noInline(applySparse);

function holey(array, ...indices) {
    for (const index of indices)
        delete array[index];
    return array;
}

function arrayStorage(array) {
    ensureArrayStorage(array);
    return array;
}

const otherRealm = createGlobalObject();

const realmWithIndexOnArrayPrototype = createGlobalObject();
realmWithIndexOnArrayPrototype.Array.prototype[1] = "array prototype";

const realmWithIndexOnObjectPrototype = createGlobalObject();
realmWithIndexOnObjectPrototype.Object.prototype[1] = "object prototype";

class DerivedArray extends Array { }

class DerivedArrayWithIndexOnPrototype extends Array { }
DerivedArrayWithIndexOnPrototype.prototype[1] = "derived prototype";

const inputs = [
    [[1, 2, 3, 4], "4:1,2,3,4"],
    [holey([1, 2, 3, 4], 0), "4:undefined,2,3,4"],
    [holey([1, 2, 3, 4], 3), "4:1,2,3,undefined"],
    [holey([1, 2, 3, 4], 1, 2), "4:1,undefined,undefined,4"],
    [holey([1, 2, 3, 4], 0, 1, 2, 3), "4:undefined,undefined,undefined,undefined"],
    [holey([1.5, 2.5, 3.5, 4.5], 0), "4:undefined,2.5,3.5,4.5"],
    [holey([1.5, 2.5, 3.5, 4.5], 3), "4:1.5,2.5,3.5,undefined"],
    [holey([1.5, NaN, 3.5, 4.5], 2), "4:1.5,NaN,undefined,4.5"],
    [holey(["a", "b", "c", "d"], 0), "4:undefined,b,c,d"],
    [holey(["a", null, undefined, "d"], 3), "4:a,null,undefined,undefined"],
    [new Array(4), "4:undefined,undefined,undefined,undefined"],
    [arrayStorage(holey([1, 2, 3, 4], 0)), "4:undefined,2,3,4"],
    [arrayStorage(holey([1, 2, 3, 4], 3)), "4:1,2,3,undefined"],
    [Object.defineProperty(holey([1, 2, 3, 4], 0), 2, { value: "read only", writable: false }), "4:undefined,2,read only,4"],
    [Object.defineProperty(holey([1, 2, 3, 4], 0), 2, { get() { return "getter"; } }), "4:undefined,2,getter,4"],
    [Object.freeze(holey([1, 2, 3, 4], 1)), "4:1,undefined,3,4"],
    [holey(otherRealm.Array(1, 2, 3, 4), 0), "4:undefined,2,3,4"],
    [Object.assign(holey([1, 2, 3, 4], 0), { named: true }), "4:undefined,2,3,4"],
    [holey(DerivedArray.of(1, 2, 3, 4), 0), "4:undefined,2,3,4"],
    [holey(DerivedArray.of(1.5, 2.5, 3.5, 4.5), 1, 3), "4:1.5,undefined,3.5,undefined"],
    [holey(DerivedArrayWithIndexOnPrototype.of(1, 2, 3, 4), 0, 1), "4:undefined,derived prototype,3,4"],
    [Object.setPrototypeOf(holey([1, 2, 3, 4], 0), null), "4:undefined,2,3,4"],
    [Object.setPrototypeOf(holey([1, 2, 3, 4], 0), {}), "4:undefined,2,3,4"],
    [Object.setPrototypeOf(holey([1, 2, 3, 4], 0, 1), { 0: "prototype" }), "4:prototype,undefined,3,4"],
    [Object.setPrototypeOf(holey([1.5, 2.5, 3.5, 4.5], 0, 1), { __proto__: Array.prototype, 1: "prototype" }), "4:undefined,prototype,3.5,4.5"],
    [Object.setPrototypeOf(new Array(4), [, "prototype"]), "4:undefined,prototype,undefined,undefined"],
    [Object.setPrototypeOf(holey([1, 2, 3, 4], 1, 3), new String("abc")), "4:1,b,3,undefined"],
    [Object.setPrototypeOf(holey([1, 2, 3, 4], 1, 3), new Uint8Array([5, 6, 7])), "4:1,6,3,undefined"],
    [Object.setPrototypeOf(holey([1, 2, 3, 4], 1, 3), new Proxy({}, { get(target, key) { return "proxy" + key; } })), "4:1,proxy1,3,proxy3"],
    [holey(realmWithIndexOnArrayPrototype.Array(1, 2, 3, 4), 0, 1), "4:undefined,array prototype,3,4"],
    [holey(realmWithIndexOnArrayPrototype.Array(1.5, 2.5, 3.5, 4.5), 1, 2), "4:1.5,array prototype,undefined,4.5"],
    [new realmWithIndexOnArrayPrototype.Array(4), "4:undefined,array prototype,undefined,undefined"],
    [arrayStorage(holey(realmWithIndexOnArrayPrototype.Array(1, 2, 3, 4), 1)), "4:1,array prototype,3,4"],
    [holey(realmWithIndexOnObjectPrototype.Array(1, 2, 3, 4), 0, 1), "4:undefined,object prototype,3,4"],
    [holey(realmWithIndexOnObjectPrototype.Array(1.5, 2.5, 3.5, 4.5), 1, 2), "4:1.5,object prototype,undefined,4.5"],
    [new realmWithIndexOnObjectPrototype.Array(4), "4:undefined,object prototype,undefined,undefined"],
];

for (let i = 0; i < testLoopCount; ++i) {
    const [array, expected] = inputs[i % inputs.length];
    shouldBe(apply(array), expected);
    shouldBe(applyInlined(array), expected);
}

const sparse = arrayStorage(holey([1, 2, 3, 4], 0));
sparse[20000] = "sparse";
for (let i = 0; i < 10; ++i)
    shouldBe(applySparse(sparse), "20001:undefined,2,undefined,sparse");

const realmWithGetterOnArrayPrototype = createGlobalObject();
let shrinking;
Object.defineProperty(realmWithGetterOnArrayPrototype.Array.prototype, 1, {
    get() {
        shrinking.length = 2;
        return "shrunk";
    },
});
for (let i = 0; i < 10; ++i) {
    shrinking = holey(realmWithGetterOnArrayPrototype.Array(1, 2, 3, 4), 1);
    shouldBe(apply(shrinking), "4:1,shrunk,undefined,undefined");
    shrinking = holey(realmWithGetterOnArrayPrototype.Array(1.5, 2.5, 3.5, 4.5), 1);
    shouldBe(apply(shrinking), "4:1.5,shrunk,undefined,undefined");
}
