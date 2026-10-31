let symbol = Symbol("symbol");

function count(object) {
    let result = 0;
    for (let key in object) {
        if (object.hasOwnProperty(key))
            result++;
    }
    return result;
}
noInline(count);

function create(i) {
    let object = { a: i, [symbol]: 100, b: 2, c: 3, d: 4, e: 5, f: 6 };
    return object;
}
noInline(create);

for (let i = 0; i < 1e6; ++i) {
    if (count(create(i)) !== 6)
        throw new Error("bad count");
}
