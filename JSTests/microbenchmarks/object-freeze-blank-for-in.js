function test() {
    let result = 0;
    for (let i = 0; i < 1e4; ++i) {
        let o = Object.freeze({ a: i, b: i, c: i, d: i });
        for (let k in o)
            result += o[k];
    }
    return result;
}
noInline(test);

let result = test();
if (result !== 4 * 1e4 * (1e4 - 1) / 2)
    throw new Error("bad result: " + result);
