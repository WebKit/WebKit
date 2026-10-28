function storeProp(key, value) {
    this[key] = value;
}

function wrap(mod) {
    const to = {};
    for (const key of Object.getOwnPropertyNames(mod))
        Object.defineProperty(to, key, { set: storeProp.bind(mod, key), enumerable: true });
    return to;
}

const firstTarget = { a: 0, b: 0, c: 0, d: 0, e: 0, f: 0 };
const secondTarget = { g: 0, h: 0, i: 0, j: 0, k: 0, l: 0 };
const first = wrap(firstTarget);
const second = wrap(secondTarget);

function test(value) {
    first.a = value;
    first.f = value;
    second.g = value;
    second.l = value;
}
noInline(test);

for (let i = 0; i < 2e6; ++i)
    test(i);

if (firstTarget.a + firstTarget.f + secondTarget.g + secondTarget.l !== (2e6 - 1) * 4)
    throw new Error("bad value");
