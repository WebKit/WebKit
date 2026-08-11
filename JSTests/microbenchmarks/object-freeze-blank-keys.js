function test() {
    let result = 0;
    for (let i = 0; i < 1e4; ++i) {
        let o = Object.freeze({ a: i, b: i, c: i, d: i });
        result += Object.keys(o).length;
        result += Object.getOwnPropertyNames(o).length;
    }
    return result;
}
noInline(test);

let result = test();
if (result !== 8e4)
    throw new Error("bad result: " + result);
