function test() {
    let result = 0;
    for (let i = 0; i < 1e4; ++i)
        result += Object.seal({ a: i, b: i + 1 }).a;
    return result;
}
noInline(test);

let result = test();
if (result !== 1e4 * (1e4 - 1) / 2)
    throw new Error("bad result: " + result);
