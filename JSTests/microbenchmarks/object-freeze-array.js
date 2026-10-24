function test() {
    let result = 0;
    for (let i = 0; i < 1e4; ++i)
        result += Object.freeze([i, i + 1, i + 2, i + 3, i + 4, i + 5, i + 6, i + 7])[0];
    return result;
}
noInline(test);

let result = test();
if (result !== 1e4 * (1e4 - 1) / 2)
    throw new Error("bad result: " + result);
