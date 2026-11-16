function accessProp(key) {
    return this[key];
}

function toESM(mod) {
    const to = {};
    for (const key of Object.getOwnPropertyNames(mod))
        Object.defineProperty(to, key, { get: accessProp.bind(mod, key), enumerable: true });
    return to;
}

const internal = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 };
const first = toESM({
    get a() { return internal.a; },
    get b() { return internal.b; },
    get c() { return internal.c; },
    get d() { return internal.d; },
    get e() { return internal.e; },
    get f() { return internal.f; },
});
const second = toESM({ g: 7, h: 8, i: 9, j: 10, k: 11, l: 12 });

const modules = [first, second];
for (let i = 0; i < 8; ++i) {
    const mod = {};
    for (let j = 0; j < 8; ++j)
        mod["m" + i + "_" + j] = 0;
    modules.push(toESM(mod));
}

for (let i = 0; i < 1e2; ++i) {
    let sum = 0;
    for (const mod of modules) {
        for (const key in mod)
            sum += mod[key];
    }
    if (sum !== 78)
        throw new Error("bad value: " + sum);
}

function test() {
    return first.a + first.f + second.g + second.l;
}
noInline(test);

for (let i = 0; i < 2e6; ++i) {
    if (test() !== 26)
        throw new Error("bad value");
}
