const v2 = Symbol.dispose;
const v4 = {
    [v2]() {
    },
};
async function* f5() {
    return yield v4;
}
const v7 = f5();
let n = 0;
async function f9(a10 = Promise, a11 = "Global") {
    for await (using v12 of v7) {
        try { a11.at(v12, ...v4); } catch (e) {}
    }
    if (n++ < testLoopCount) f9(Promise, Symbol);
}
f9();
