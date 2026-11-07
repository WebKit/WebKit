function test(object, value) {
    const keys = Object.keys(object);
    keys.push(value);
    return keys;
}
noInline(test);

const object = { a: 1, b: 2, c: 3 };

for (let i = 0; i < 1e6; i++) {
    const result = test(object, "d");
    if (result.length !== 4 || result[0] !== "a" || result[3] !== "d")
        throw new Error("bad result");
}
