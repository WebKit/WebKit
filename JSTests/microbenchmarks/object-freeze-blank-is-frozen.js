function test() {
    let result = 0;
    for (let i = 0; i < 1e4; ++i) {
        let o = Object.freeze({ a: i, b: i + 1 });
        if (Object.isFrozen(o))
            ++result;
        if (Object.isSealed(o))
            ++result;
    }
    return result;
}
noInline(test);

let result = test();
if (result !== 2e4)
    throw new Error("bad result: " + result);
