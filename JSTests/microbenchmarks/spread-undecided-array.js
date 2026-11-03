function test(array) {
    return [...array];
}
noInline(test);

const array = new Array(64);

for (let i = 0; i < 2e5; i++) {
    if (test(array).length !== 64)
        throw new Error("bad length");
}
