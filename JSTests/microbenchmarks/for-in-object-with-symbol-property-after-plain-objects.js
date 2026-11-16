let symbol = Symbol("symbol");

function sum(object) {
    let result = 0;
    for (let key in object)
        result += object[key];
    return result;
}
noInline(sum);

function createPlain(i) {
    return { a: i, b: 2, c: 3, d: 4, e: 5, f: 6 };
}
noInline(createPlain);

function create(i) {
    return { a: i, [symbol]: 100, b: 2, c: 3, d: 4, e: 5, f: 6 };
}
noInline(create);

for (let i = 0; i < 5e5; ++i) {
    if (sum(createPlain(i)) !== i + 20)
        throw new Error("bad sum");
}

for (let i = 0; i < 1e6; ++i) {
    if (sum(create(i)) !== i + 20)
        throw new Error("bad sum");
}
