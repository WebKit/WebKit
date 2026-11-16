function test(iterable) {
    return [...iterable];
}
noInline(test);

const array = [];
for (let i = 0; i < 64; i++)
    array.push("value" + i);

for (let i = 0; i < 1e6; i++) {
    if (test(array).length !== 64)
        throw new Error("bad length");
    test("a");
}
