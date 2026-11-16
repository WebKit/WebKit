function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

function fold(result, x) {
    return result * 100 + (typeof x === "object" ? x[0] * 10 + x[1] : +x);
}
noDFG(fold);

function reference(iterable) {
    let iterator = iterable[Symbol.iterator]();
    let next = iterator.next;
    let result = 0;
    for (;;) {
        let step = next.call(iterator);
        if (step.done)
            return result;
        result = fold(result, step.value);
    }
}
noDFG(reference);

function run(f, iterable) {
    try {
        return f(iterable);
    } catch (error) {
        return error instanceof TypeError ? "TypeError" : "unexpected " + error;
    }
}
noDFG(run);

let siteCount = 0;
function makeSite() {
    let site = new Function("iterable", `
        let result = 0;
        for (let x of iterable)
            result = fold(result, x);
        return result;
        // ${siteCount++}`);
    noInline(site);
    return site;
}

function withProperty(object, key, value) {
    object[key] = value;
    return object;
}

let array = [1, 2, 3];
let map = new Map([[1, 4], [2, 5]]);
let set = new Set([6, 7, 8]);
let kinds = {
    arrayValues: () => array.values(),
    arrayKeys: () => array.keys(),
    map: () => map,
    set: () => set,
    string: () => "123",
    generic: () => ({
        [Symbol.iterator]() {
            let i = 0;
            return { next() { return i < 2 ? { value: ++i, done: false } : { value: undefined, done: true }; } };
        }
    }),
};

let strangers = [
    () => ({ [Symbol.iterator]: Map.prototype.entries }),
    () => ({ [Symbol.iterator]: Set.prototype.values }),
    () => ({ [Symbol.iterator]: String.prototype[Symbol.iterator], toString() { return "45"; } }),
    () => withProperty(new Set([1]), Symbol.iterator, Map.prototype.entries),
    () => withProperty(new Map([[1, 2]]), Symbol.iterator, Set.prototype.values),
    () => withProperty([12], Symbol.iterator, String.prototype[Symbol.iterator]),
    () => new String("67"),
    () => new Uint8Array([1, 2]).values(),
    () => new Uint8Array([1, 2]).keys(),
    () => Array.prototype.values.call({ length: 2, 0: 3, 1: 4 }),
];

let sites = [
    ["arrayValues", "arrayKeys"],
    ["map", "set"],
    ["set", "string"],
    ["string", "generic"],
];

function test(names) {
    let site = makeSite();
    for (let i = 0; i < testLoopCount; ++i) {
        let name = names[i % names.length];
        shouldBe(site(kinds[name]()), expectedForKinds[name], names + " with " + name);
    }
    for (let name in kinds)
        shouldBe(run(site, kinds[name]()), expectedForKinds[name], names + " with " + name);
    for (let i = 0; i < strangers.length; ++i)
        shouldBe(run(site, strangers[i]()), expectedForStrangers[i], names + " with stranger " + i);
}
noDFG(test);

let expectedForKinds = { };
for (let name in kinds)
    expectedForKinds[name] = run(reference, kinds[name]());
let expectedForStrangers = strangers.map(stranger => run(reference, stranger()));

for (let names of sites)
    test(names);
